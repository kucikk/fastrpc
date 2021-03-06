/*
 * FastRPC -- Fast RPC library compatible with XML-RPC
 * Copyright (C) 2005-7  Seznam.cz, a.s.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Seznam.cz, a.s.
 * Radlicka 2, Praha 5, 15000, Czech Republic
 * http://www.seznam.cz, mailto:fastrpc@firma.seznam.cz
 *
 * FILE          $Id: frpcserverproxy.cc,v 1.11 2011-02-18 10:37:45 skeleton-golem Exp $
 *
 * DESCRIPTION
 *
 * AUTHOR
 *              Miroslav Talasek <miroslav.talasek@firma.seznam.cz>
 *
 * HISTORY
 *
 */

#include <sstream>
#include <memory>

#include <stdarg.h>

#include "frpcserverproxy.h"
#include <frpc.h>
#include <frpctreebuilder.h>
#include <frpctreefeeder.h>
#include <memory>
#include <frpcfault.h>
#include <frpcresponseerror.h>

#include <frpcstruct.h>
#include <frpcstring.h>
#include <frpcint.h>
#include <frpcbool.h>


namespace {
    FRPC::Pool_t localPool;

    int getTimeout(const FRPC::Struct_t &config, const std::string &name,
                   int defaultValue)
    {
        // get key from config and check for existence
        const FRPC::Value_t *val(config.get(name));
        if (!val) return defaultValue;

        // OK
        return FRPC::Int(*val);
    }

    FRPC::ProtocolVersion_t parseProtocolVersion(const FRPC::Struct_t &config,
                                                 const std::string &name)
    {
        std::string strver
            (FRPC::String(config.get(name, FRPC::String_t::FRPC_EMPTY)));
        // empty/undefined => current version
        if (strver.empty()) return FRPC::ProtocolVersion_t();

        // parse input
        std::istringstream is(strver);
        int major, minor;
        is >> major >> minor;

        // OK
        return FRPC::ProtocolVersion_t(major, minor);
    }

    FRPC::ServerProxy_t::Config_t configFromStruct(const FRPC::Struct_t &s)
    {
        FRPC::ServerProxy_t::Config_t config;

        config.proxyUrl = FRPC::String(s.get("proxyUrl", FRPC::String_t::FRPC_EMPTY));
        config.readTimeout = getTimeout(s, "readTimeout", 10000);
        config.writeTimeout = getTimeout(s, "writeTimeout", 1000);
        config.useBinary = FRPC::Int(s.get("transferMode", FRPC::Int_t::FRPC_ZERO));
        config.useHTTP10 = FRPC::Bool(s.get("useHTTP10", FRPC::Bool_t::FRPC_FALSE));
        config.protocolVersion = parseProtocolVersion(s, "protocolVersion");
        config.connectTimeout = getTimeout(s, "connectTimeout", 10000);
        config.keepAlive = FRPC::Bool(s.get("keepAlive", FRPC::Bool_t::FRPC_FALSE));

        return config;
    }

    FRPC::Connector_t* makeConnector(
        const FRPC::URL_t &url,
        const unsigned &connectTimeout,
        const bool &keepAlive)
    {
        if (url.isUnix()) {
            return new FRPC::SimpleConnectorUnix_t(
               url, connectTimeout, keepAlive);
        }
        return new FRPC::SimpleConnectorIPv6_t(url, connectTimeout, keepAlive);
    }
}

namespace FRPC {

class ServerProxyImpl_t {
public:
    ServerProxyImpl_t(const std::string &server,
                      const ServerProxy_t::Config_t &config)
        : url(server, config.proxyUrl),
          io(-1, config.readTimeout, config.writeTimeout, -1 ,-1),
          rpcTransferMode(config.useBinary), useHTTP10(config.useHTTP10),
          serverSupportedProtocols(HTTPClient_t::XML_RPC),
          protocolVersion(config.protocolVersion),
          connector(makeConnector(url, config.connectTimeout,
                                          config.keepAlive))
    {}

    /** Set new read timeout */
    void setReadTimeout(int timeout) {
        io.setReadTimeout(timeout);
    }

    /** Set new write timeout */
    void setWriteTimeout(int timeout) {
        io.setWriteTimeout(timeout);
    }

    /** Set new connect timeout */
    void setConnectTimeout(int timeout) {
        connector->setTimeout(timeout);
    }

    const URL_t& getURL() {
        return url;
    }

    /** Create marshaller.
     */
    Marshaller_t* createMarshaller(HTTPClient_t &client);

    /** Call method.
     */
    Value_t& call(Pool_t &pool, const std::string &methodName,
                  const Array_t &params);

    void call(DataBuilder_t &builder, const std::string &methodName,
                                 const Array_t &params);

    /** Call method with variable number of arguments.
     */
    Value_t& call(Pool_t &pool, const char *methodName, va_list args);

    void addRequestHttpHeaderForCall(const HTTPClient_t::Header_t& header);
    void addRequestHttpHeaderForCall(const HTTPClient_t::HeaderVector_t& headers);

    void addRequestHttpHeader(const HTTPClient_t::Header_t& header);
    void addRequestHttpHeader(const HTTPClient_t::HeaderVector_t& headers);

    void deleteRequestHttpHeaders();

private:
    URL_t url;
    HTTPIO_t io;
    unsigned int rpcTransferMode;
    bool useHTTP10;
    unsigned int serverSupportedProtocols;
    ProtocolVersion_t protocolVersion;
    std::auto_ptr<Connector_t> connector;
    HTTPClient_t::HeaderVector_t requestHttpHeadersForCall;
    HTTPClient_t::HeaderVector_t requestHttpHeaders;
};

Marshaller_t* ServerProxyImpl_t::createMarshaller(HTTPClient_t &client) {
    Marshaller_t *marshaller;
    switch (rpcTransferMode) {
    case ServerProxy_t::Config_t::ON_SUPPORT:
        {
            if (serverSupportedProtocols & HTTPClient_t::BINARY_RPC) {
                //using BINARY_RPC
                marshaller= Marshaller_t::create(Marshaller_t::BINARY_RPC,
                                                 client,protocolVersion);
                client.prepare(HTTPClient_t::BINARY_RPC);
            } else {
                //using XML_RPC
                marshaller = Marshaller_t::create
                    (Marshaller_t::XML_RPC,client, protocolVersion);
                client.prepare(HTTPClient_t::XML_RPC);
            }
        }
    break;

    case ServerProxy_t::Config_t::NEVER:
        {
            // never using BINARY_RPC
            marshaller= Marshaller_t::create(Marshaller_t::XML_RPC,
                                             client,protocolVersion);
            client.prepare(HTTPClient_t::XML_RPC);
        }
    break;

    case ServerProxy_t::Config_t::ALWAYS:
        {
            //using BINARY_RPC  always
            marshaller= Marshaller_t::create(Marshaller_t::BINARY_RPC,
                                             client,protocolVersion);
            client.prepare(HTTPClient_t::BINARY_RPC);
        }
    break;

    case ServerProxy_t::Config_t::ON_SUPPORT_ON_KEEP_ALIVE:
    default:
        {
            if ((serverSupportedProtocols & HTTPClient_t::XML_RPC)
                || connector->getKeepAlive() == false
                || io.socket() != -1) {
                //using XML_RPC
                marshaller= Marshaller_t::create
                    (Marshaller_t::XML_RPC,client, protocolVersion);
                client.prepare(HTTPClient_t::XML_RPC);
            } else {
                //using BINARY_RPC
                marshaller= Marshaller_t::create
                    (Marshaller_t::BINARY_RPC, client,protocolVersion);
                client.prepare(HTTPClient_t::BINARY_RPC);
            }
        }
        break;
    }

    // OK
    return marshaller;
}

ServerProxy_t::ServerProxy_t(const std::string &server, const Config_t &config)
    : sp(new ServerProxyImpl_t(server, config))
{}

ServerProxy_t::ServerProxy_t(const std::string &server, const Struct_t &config)
    : sp(new ServerProxyImpl_t(server, configFromStruct(config)))
{}


ServerProxy_t::~ServerProxy_t() {
    // get rid of implementation
}

Value_t& ServerProxyImpl_t::call(Pool_t &pool, const std::string &methodName,
                                 const Array_t &params)
{
    HTTPClient_t client(io, url, connector.get(), useHTTP10);
    {
        client.addCustomRequestHeader(requestHttpHeaders);
        client.addCustomRequestHeader(requestHttpHeadersForCall);
        requestHttpHeadersForCall.clear();
    }
    TreeBuilder_t builder(pool);
    std::auto_ptr<Marshaller_t>marshaller(createMarshaller(client));
    TreeFeeder_t feeder(*marshaller);

    try {
        marshaller->packMethodCall(methodName.c_str());
        for (Array_t::const_iterator
                 iparams = params.begin(),
                 eparams = params.end();
             iparams != eparams; ++iparams) {
            feeder.feedValue(**iparams);
        }

        marshaller->flush();
    } catch (const ResponseError_t &e) {}

    client.readResponse(builder);
    serverSupportedProtocols = client.getSupportedProtocols();
    protocolVersion = client.getProtocolVersion();

    // OK, return unmarshalled data (throws fault if NULL)
    return builder.getUnMarshaledData();
}


void ServerProxyImpl_t::call(DataBuilder_t &builder, const std::string &methodName,
                                 const Array_t &params)
{
    HTTPClient_t client(io, url, connector.get(), useHTTP10);
    {
        client.addCustomRequestHeader(requestHttpHeaders);
        client.addCustomRequestHeader(requestHttpHeadersForCall);
        requestHttpHeadersForCall.clear();
    }
    std::auto_ptr<Marshaller_t>marshaller(createMarshaller(client));
    TreeFeeder_t feeder(*marshaller);

    try {
        marshaller->packMethodCall(methodName.c_str());
        for (Array_t::const_iterator
                 iparams = params.begin(),
                 eparams = params.end();
             iparams != eparams; ++iparams) {
            feeder.feedValue(**iparams);
        }

        marshaller->flush();
    } catch (const ResponseError_t &e) {}

    client.readResponse(builder);
    serverSupportedProtocols = client.getSupportedProtocols();
    protocolVersion = client.getProtocolVersion();
}


Value_t& ServerProxy_t::call(Pool_t &pool, const std::string &methodName,
                             const Array_t &params)
{
    return sp->call(pool, methodName, params);
}

Value_t& ServerProxyImpl_t::call(Pool_t &pool, const char *methodName,
                                 va_list args)
{
    HTTPClient_t client(io, url, connector.get(), useHTTP10);
    {
        client.addCustomRequestHeader(requestHttpHeaders);
        client.addCustomRequestHeader(requestHttpHeadersForCall);
        requestHttpHeadersForCall.clear();
    }
    TreeBuilder_t builder(pool);
    std::auto_ptr<Marshaller_t>marshaller(createMarshaller(client));
    TreeFeeder_t feeder(*marshaller);

    try {
        marshaller->packMethodCall(methodName);

        // marshall all passed values until null pointer
        while (const Value_t *value = va_arg(args, Value_t*))
            feeder.feedValue(*value);

        marshaller->flush();
    } catch (const ResponseError_t &e) {}

    client.readResponse(builder);
    serverSupportedProtocols = client.getSupportedProtocols();
    protocolVersion = client.getProtocolVersion();

    // OK, return unmarshalled data (throws fault if NULL)
    return builder.getUnMarshaledData();
}

void ServerProxyImpl_t::addRequestHttpHeaderForCall(const HTTPClient_t::Header_t& header)
{
    requestHttpHeadersForCall.push_back(header);
}

void ServerProxyImpl_t::addRequestHttpHeaderForCall(const HTTPClient_t::HeaderVector_t& headers)
{
    for (HTTPClient_t::HeaderVector_t::const_iterator it=headers.begin(); it!=headers.end(); it++) {
        addRequestHttpHeaderForCall(*it);
    }
}

void ServerProxyImpl_t::addRequestHttpHeader(const HTTPClient_t::Header_t& header)
{
    requestHttpHeaders.push_back(header);
}

void ServerProxyImpl_t::addRequestHttpHeader(const HTTPClient_t::HeaderVector_t& headers)
{
    for (HTTPClient_t::HeaderVector_t::const_iterator it=headers.begin(); it!=headers.end(); it++) {
        addRequestHttpHeader(*it);
    }
}

void ServerProxyImpl_t::deleteRequestHttpHeaders() {
    requestHttpHeaders.clear();
    requestHttpHeadersForCall.clear();
}

namespace {
    /** Hold va_list and destroy it (via va_end) on destruction.
     */
    struct VaListHolder_t {
        VaListHolder_t(va_list &args) : args(args) {}
        ~VaListHolder_t() { va_end(args); }
        va_list &args;
    };
}

Value_t& ServerProxy_t::call(Pool_t &pool, const char *methodName, ...) {
    // get variadic arguments
    va_list args;
    va_start(args, methodName);
    VaListHolder_t argsHolder(args);

    // use implementation
    return sp->call(pool, methodName, args);
}

void ServerProxy_t::call(DataBuilder_t &builder,
        const std::string &methodName, const Array_t &params)
{
    sp->call(builder, methodName, params);
}

void ServerProxy_t::setReadTimeout(int timeout) {
    sp->setReadTimeout(timeout);
}

void ServerProxy_t::setWriteTimeout(int timeout) {
    sp->setWriteTimeout(timeout);
}

void ServerProxy_t::setConnectTimeout(int timeout) {
    sp->setConnectTimeout(timeout);
}

void ServerProxy_t::setForwardHeader(const std::string &fwd) {
    sp->addRequestHttpHeaderForCall(std::make_pair(HTTP_HEADER_X_FORWARDED_FOR, fwd));
}

const URL_t& ServerProxy_t::getURL() {
    return sp->getURL();
}

void ServerProxy_t::addRequestHttpHeaderForCall(const HTTPClient_t::Header_t& header) {
    sp->addRequestHttpHeaderForCall(header);
}
void ServerProxy_t::addRequestHttpHeaderForCall(const HTTPClient_t::HeaderVector_t& headers) {
    sp->addRequestHttpHeaderForCall(headers);
}

void ServerProxy_t::addRequestHttpHeader(const HTTPClient_t::Header_t& header) {
    sp->addRequestHttpHeader(header);
}

void ServerProxy_t::addRequestHttpHeader(const HTTPClient_t::HeaderVector_t& headers) {
    sp->addRequestHttpHeader(headers);
}

void ServerProxy_t::deleteRequestHttpHeaders() {
    sp->deleteRequestHttpHeaders();
}

} // namespace FRPC
