/* Copyright (C) 2014 InfiniDB, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA. */

/****************************************************************************
* $Id: func_regexp.cpp 3495 2013-01-21 14:09:51Z rdempsey $
*
*
****************************************************************************/

#include <cstdlib>
#include <string>
using namespace std;

#include "regex-port.h"

#include "functor_bool.h"
#include "functioncolumn.h"
#include "predicateoperator.h"
#include "constantcolumn.h"
using namespace execplan;

#include "rowgroup.h"

#include "errorcodes.h"
#include "idberrorinfo.h"
#include "errorids.h"
using namespace logging;

namespace
{
inline bool getBool(rowgroup::Row& row,
                    funcexp::FunctionParm& pm,
                    bool& isNull,
                    CalpontSystemCatalog::ColType& ct,
                    const string& timeZone)
{

    string expr;
    string pattern;

    switch (pm[0]->data()->resultType().colDataType)
    {
        case execplan::CalpontSystemCatalog::BIGINT:
        case execplan::CalpontSystemCatalog::INT:
        case execplan::CalpontSystemCatalog::MEDINT:
        case execplan::CalpontSystemCatalog::TINYINT:
        case execplan::CalpontSystemCatalog::SMALLINT:
        case execplan::CalpontSystemCatalog::UBIGINT:
        case execplan::CalpontSystemCatalog::UINT:
        case execplan::CalpontSystemCatalog::UMEDINT:
        case execplan::CalpontSystemCatalog::UTINYINT:
        case execplan::CalpontSystemCatalog::USMALLINT:
        case execplan::CalpontSystemCatalog::VARCHAR: // including CHAR'
        case execplan::CalpontSystemCatalog::CHAR:
        case execplan::CalpontSystemCatalog::TEXT:
        case execplan::CalpontSystemCatalog::DOUBLE:
        case execplan::CalpontSystemCatalog::UDOUBLE:
        case execplan::CalpontSystemCatalog::FLOAT:
        case execplan::CalpontSystemCatalog::UFLOAT:
        {
            expr = pm[0]->data()->getStrVal(row, isNull);
            break;
        }

        case execplan::CalpontSystemCatalog::DATE:
        {
            expr = dataconvert::DataConvert::dateToString(pm[0]->data()->getDateIntVal(row, isNull));
            break;
        }

        case execplan::CalpontSystemCatalog::DATETIME:
        {
            expr = dataconvert::DataConvert::datetimeToString(pm[0]->data()->getDatetimeIntVal(row, isNull));
            //strip off micro seconds
            expr = expr.substr(0, 19);
            break;
        }

        case execplan::CalpontSystemCatalog::TIMESTAMP:
        {
            expr = dataconvert::DataConvert::timestampToString(pm[0]->data()->getTimestampIntVal(row, isNull), timeZone);
            //strip off micro seconds
            expr = expr.substr(0, 19);
            break;
        }

        case execplan::CalpontSystemCatalog::TIME:
        {
            expr = dataconvert::DataConvert::timeToString(pm[0]->data()->getTimeIntVal(row, isNull));
            //strip off micro seconds
            expr = expr.substr(0, 19);
            break;
        }

        case execplan::CalpontSystemCatalog::DECIMAL:
        case execplan::CalpontSystemCatalog::UDECIMAL:
        {
            IDB_Decimal d = pm[0]->data()->getDecimalVal(row, isNull);

            if (pm[0]->data()->resultType().colWidth == datatypes::MAXDECIMALWIDTH)
            {
                expr = d.toString(true);
            }
            else
            {
                expr = d.toString();
            }

            break;
        }

        default:
        {
            std::ostringstream oss;
            oss << "regexp: datatype of " << execplan::colDataTypeToString(ct.colDataType);
            throw logging::IDBExcept(oss.str(), ERR_DATATYPE_NOT_SUPPORT);
        }
    }

    if (pm[1]->data()->isConstant())
    {
        ConstantColumn* cc = dynamic_cast<ConstantColumn*>(pm[1]->data());
        if (cc) {
            if (!cc->directRegex())
	    {
                cc->constructRegex();
	    }
            return cc->directRegex()->matchSubstring(expr.c_str());
	}
    }
    // XXX Pattern is often constant, and we should prefer plans where
    //     pattern changes less often than expression.
    switch (pm[1]->data()->resultType().colDataType)
    {
        case execplan::CalpontSystemCatalog::BIGINT:
        case execplan::CalpontSystemCatalog::INT:
        case execplan::CalpontSystemCatalog::MEDINT:
        case execplan::CalpontSystemCatalog::TINYINT:
        case execplan::CalpontSystemCatalog::SMALLINT:
        case execplan::CalpontSystemCatalog::UBIGINT:
        case execplan::CalpontSystemCatalog::UINT:
        case execplan::CalpontSystemCatalog::UMEDINT:
        case execplan::CalpontSystemCatalog::UTINYINT:
        case execplan::CalpontSystemCatalog::USMALLINT:
        case execplan::CalpontSystemCatalog::VARCHAR: // including CHAR'
        case execplan::CalpontSystemCatalog::DOUBLE:
        case execplan::CalpontSystemCatalog::UDOUBLE:
        case execplan::CalpontSystemCatalog::FLOAT:
        case execplan::CalpontSystemCatalog::UFLOAT:
        case execplan::CalpontSystemCatalog::CHAR:
        case execplan::CalpontSystemCatalog::TEXT:
        {
            pattern = pm[1]->data()->getStrVal(row, isNull);
            break;
        }

        case execplan::CalpontSystemCatalog::DATE:
        {
            pattern = dataconvert::DataConvert::dateToString(pm[1]->data()->getDateIntVal(row, isNull));
            break;
        }

        case execplan::CalpontSystemCatalog::DATETIME:
        {
            pattern = dataconvert::DataConvert::datetimeToString(pm[1]->data()->getDatetimeIntVal(row, isNull));
            //strip off micro seconds
            pattern = pattern.substr(0, 19);
            break;
        }

        case execplan::CalpontSystemCatalog::TIMESTAMP:
        {
            pattern = dataconvert::DataConvert::timestampToString(pm[1]->data()->getTimestampIntVal(row, isNull), timeZone);
            //strip off micro seconds
            pattern = pattern.substr(0, 19);
            break;
        }

        case execplan::CalpontSystemCatalog::TIME:
        {
            pattern = dataconvert::DataConvert::timeToString(pm[1]->data()->getTimeIntVal(row, isNull));
            //strip off micro seconds
            pattern = pattern.substr(0, 19);
            break;
        }

        case execplan::CalpontSystemCatalog::DECIMAL:
        case execplan::CalpontSystemCatalog::UDECIMAL:
        {
            IDB_Decimal d = pm[1]->data()->getDecimalVal(row, isNull);

            if (pm[1]->data()->resultType().colWidth == datatypes::MAXDECIMALWIDTH)
            {
                pattern = d.toString(true);
            }
            else
            {
                pattern = d.toString();
            }
            break;
        }

        default:
        {
            std::ostringstream oss;
            oss << "regexp: datatype of " << execplan::colDataTypeToString(ct.colDataType);
            throw logging::IDBExcept(oss.str(), ERR_DATATYPE_NOT_SUPPORT);
        }
    }

    // XXX This thing does regex compilation on every call, even for constant patterns.
    //     This means that for "SELECT ... WHERE a RLiKE b" or "SELECT ... WHERE REGEXP(a, b)"
    //     we add non-negligible overhead of regex compilation.
    //     It is especially pronounced for Google's re2 library. For POSIX regexes the overhead is
    //     100%, approximately: "a RLIKE 'bubu'" executes about three times slower than "a LIKE '%bubu%'".
    utils::mcs_regex_t re;
    re.compile(pattern.c_str());
    return re.matchSubstring(expr.c_str());

}

}

namespace funcexp
{

CalpontSystemCatalog::ColType Func_regexp::operationType( FunctionParm& fp, CalpontSystemCatalog::ColType& resultType )
{
    return resultType;
}

bool Func_regexp::getBoolVal(rowgroup::Row& row,
                             FunctionParm& pm,
                             bool& isNull,
                             CalpontSystemCatalog::ColType& ct)
{
    return getBool(row, pm, isNull, ct, timeZone()) && !isNull;
}


} // namespace funcexp
// vim:ts=4 sw=4:
