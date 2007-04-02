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
 * $Id: pythonbuilder.cc,v 1.3 2007-04-02 15:42:58 vasek Exp $
 *
 * AUTHOR      Vaclav Blazek <blazek@firma.seznam.cz>
 *
 * DESCRIPTION
 * Python FastRPC support. Mimics C++ FastRPC server and method registry.
 *
 * HISTORY
 *      2006-05-24 (vasek)
 *              Created
 */


#include "pythonbuilder.h"
#include "fastrpcmodule.h"

using namespace FRPC::Python;

namespace FRPC { namespace Python {
StringMode_t parseStringMode(const char *stringMode) {
    if (stringMode) {
        if (!strcmp(stringMode, "string")) {
            return STRING_MODE_STRING;
        } else if (!strcmp(stringMode, "unicode")) {
            return STRING_MODE_UNICODE;
        } else if (strcmp(stringMode, "mixed")) {
            PyErr_Format(PyExc_ValueError,
                         "Invalid valu '%s' of stringMode.", stringMode);
            return STRING_MODE_INVALID;
        }
    }

    // default
    return STRING_MODE_MIXED;
}

} } // namespace FRPC::Python

void Builder_t::buildMethodResponse()
{}
void Builder_t::buildBinary(const char* data, long size)
{
    if(isError())
        return;
    PyObject *binary = reinterpret_cast<PyObject*>(newBinary(data, size));

    if(!binary)
        setError();
    
    if(!isMember(binary))
        isFirst(binary);

}
void Builder_t::buildBinary(const std::string &data)
{
    if(isError())
        return;
    PyObject *binary = reinterpret_cast<PyObject*>(newBinary(data.data(),
                       data.size()));
    if(!binary)
        setError();
                       
    if(!isMember(binary))
        isFirst(binary);
}
void Builder_t::buildBool(bool value)
{
    if(isError())
        return;
    PyObject *boolean = reinterpret_cast<PyObject*>(newBoolean(value));

    if(!boolean)
        setError();
        
    if(!isMember(boolean))
        isFirst(boolean);
}
void Builder_t::buildDateTime(short year, char month, char day,char hour, char
                              min, char sec,
                              char weekDay, time_t unixTime, char timeZone)
{
    if(isError())
        return;
    
    PyObject *dateTime = reinterpret_cast<PyObject*>(newDateTime(year, month,
                         day, hour, min,
                         sec, weekDay, unixTime, timeZone) );
    if(!dateTime)
        setError();
        
    if(!isMember(dateTime))
        isFirst(dateTime);
}
void Builder_t::buildDouble(double value)
{
    if(isError())
        return;
    PyObject *doubleVal = PyFloat_FromDouble(value);

    if(!doubleVal)
        setError();
    
    if(!isMember(doubleVal))
        isFirst(doubleVal);

}
void Builder_t::buildFault(long errNumber, const char* errMsg, long size)
{
    if(isError())
        return;
    PyObject *args =Py_BuildValue("iNO",errNumber,
                                  PyString_FromStringAndSize(errMsg,size),
                                  methodObject ? methodObject : Py_None);
    PyObject *fault = PyInstance_New(Fault, args, 0);
    //isFirst(fault);
    
    if(!fault)
        setError();
    
    Py_XDECREF(retValue);
    retValue = fault;
    first = true;

}
void Builder_t::buildFault(long errNumber, const std::string &errMsg)
{
    if(isError())
        return;
    PyObject *args =Py_BuildValue("isO", errNumber, errMsg.c_str(),
                                  methodObject ? methodObject : Py_None);
    PyObject *fault = PyInstance_New(Fault, args, 0);
        
    if(!fault)
        setError();
        
    Py_XDECREF(retValue);
    retValue = fault;
    first = true;
}
void Builder_t::buildInt(long value)
{
    if(isError())
        return;
    PyObject *integer = PyInt_FromLong(value);

    if(!integer)
        setError();
        
    if(!isMember(integer))
        isFirst(integer);

}
void Builder_t::buildMethodCall(const char* methodName1, long size) {
    if (methodName) delete methodName;
    methodName = new std::string(methodName1, size);

    PyObject *params = PyList_New(0);
    if (!params) setError();

    isFirst(params);
    entityStorage.push_back(TypeStorage_t(params, ARRAY));
}

void Builder_t::buildMethodCall(const std::string &methodName1) {
    if (methodName) delete methodName;
    methodName = new std::string(methodName1);
    
    PyObject *params = PyList_New(0);
    if (!params) setError();

    isFirst(params);
    entityStorage.push_back(TypeStorage_t(params, ARRAY));
}

void Builder_t::buildString(const char* data, long size)
{
    if(isError())
        return;
    bool utf8 = (stringMode == STRING_MODE_UNICODE);

    // check 8-bit string only iff mixed
    if (stringMode == STRING_MODE_MIXED) {
        for(long i = 0; i < size; i++) {
            if(data[i] & 0x80) {
                utf8 = true;
                break;
            }
        }
    }

    PyObject *stringVal;

    if (utf8) {
        stringVal = PyUnicode_DecodeUTF8(data, size, "strict");
    } else {
        stringVal = PyString_FromStringAndSize(const_cast<char*>(data), size);
    }

    if(!stringVal)
        setError();

    if(!isMember(stringVal))
        isFirst(stringVal);
}

void Builder_t::buildStructMember(const char *memberName, long size )
{
    if(isError())
        return;
    std::string buf(memberName,size);
    this->memberName =  buf;

}
void Builder_t::buildStructMember(const std::string &memberName)
{
    if(isError())
        return;
    this->memberName = memberName;
}
void Builder_t::closeArray()
{
    if(isError())
        return;
    entityStorage.pop_back();
}
void Builder_t::closeStruct()
{
    if(isError())
        return;
    entityStorage.pop_back();
}
void Builder_t::openArray(long numOfItems)
{
    if(isError())
        return;
    PyObject *array = PyList_New(0);

    if(!array)
        setError();
    
    if(!isMember(array))
        isFirst(array);

    entityStorage.push_back(TypeStorage_t(array,ARRAY));
}
void Builder_t::openStruct(long numOfMembers)
{

    if(isError())
        return;
    PyObject *structVal = PyDict_New();

    if(!structVal)
        setError();
    
    if(!isMember(structVal))
        isFirst(structVal);

    entityStorage.push_back(TypeStorage_t(structVal,STRUCT));
}
