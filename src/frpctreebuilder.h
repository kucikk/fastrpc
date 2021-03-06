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
 * FILE          $Id: frpctreebuilder.h,v 1.5 2010-04-21 08:48:03 edois Exp $
 *
 * DESCRIPTION   
 *
 * AUTHOR        
 *              Miroslav Talasek <miroslav.talasek@firma.seznam.cz>
 *
 * HISTORY
 *       
 */
#ifndef FRPCFRPCTREEBUILDER_H
#define FRPCFRPCTREEBUILDER_H

#include <frpcplatform.h>

#include <frpcdatabuilder.h>
#include <frpc.h>
#include <frpcfault.h>

namespace FRPC
{
struct ValueTypeStorage_t
{

    ValueTypeStorage_t(Value_t *container, char type):type(type),container(container)
    {}
    ~ValueTypeStorage_t()
    {}
    char type;
    Value_t* container;
};

/**
@author Miroslav Talasek
*/
class Pool_t;

class FRPC_DLLEXPORT TreeBuilder_t : public DataBuilder_t
{
public:
    TreeBuilder_t(Pool_t &pool):DataBuilder_t(),
                  pool(pool),first(true),retValue(0),errNum(-500)
    {}
    enum{ARRAY=0,STRUCT};
    virtual ~TreeBuilder_t();

    virtual void buildBinary(const char* data, unsigned int size);
    virtual void buildBinary(const std::string& data);
    virtual void buildBool(bool value);
    virtual void buildDateTime(short year, char month, char day, 
                               char hour, char min, char sec, char weekDay,
                                time_t unixTime, int timeZone);
    virtual void buildDouble(double value);
    virtual void buildFault(int errNumber, const char* errMsg, unsigned int size);
    virtual void buildFault(int errNumber, const std::string& errMsg);
    virtual void buildInt(Int_t::value_type value);
    virtual void buildMethodCall(const char* methodName, unsigned int size);
    virtual void buildMethodCall(const std::string& methodName);
    virtual void buildMethodResponse();
    virtual void buildString(const char* data, unsigned int size);
    virtual void buildString(const std::string& data);
    virtual void buildStructMember(const char* memberName, unsigned int size);
    virtual void buildStructMember(const std::string& memberName);
    virtual void closeArray();
    virtual void closeStruct();
    virtual void openArray(unsigned int numOfItems);
    virtual void openStruct(unsigned int numOfMembers);
    void buildNull();
    inline bool isFirst( Value_t  &value )
    {
        if(first)
        {
            retValue = &value;
            first = false;
            return true;
        }
        return false;
    }
    inline bool isMember(Value_t &value )
    {
        if(entityStorage.size() < 1)
            return false;
        switch(entityStorage.back().type)
        {
        case ARRAY:
            {

                dynamic_cast<Array_t*>(entityStorage.back().container)->append(value);

                //entityStorage.back().numOfItems--;
            }
            break;
        case STRUCT:
            {
                dynamic_cast<Struct_t*>(entityStorage.back().container)->
                        append(memberName ,value);


                //entityStorage.back().numOfItems--;

            }
            break;
        default:
            //OOPS
            break;

        }
        /*if(entityStorage.back().numOfItems == 0)
        entityStorage.pop_back();*/
        return true;
    }

    inline Value_t& getUnMarshaledData()
    {
        if (!retValue)
            throw Fault_t(getUnMarshaledErrorNumber(),
                          getUnMarshaledErrorMessage());
        return *retValue;
    }

    inline Value_t* getUnMarshaledDataPtr()
    {
        return retValue;
    }

    inline const std::string getUnMarshaledMethodName()
    {
        return methodName;
    }

    inline const std::string getUnMarshaledErrorMessage()
    {
        if(errMsg.size() != 0)
            return errMsg;
        else
            return "No data unmarshalled";
    }

    inline long getUnMarshaledErrorNumber()
    {
        return errNum;
    }

private:
    Pool_t &pool;
    bool first;
    Value_t *retValue;
    std::string memberName;
    std::string methodName;
    int errNum;
    std::string errMsg;
    std::vector<ValueTypeStorage_t> entityStorage;
};

};

#endif
