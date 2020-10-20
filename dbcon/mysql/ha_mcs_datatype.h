/*
   Copyright (C) 2020 MariaDB Corporation

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
#ifndef HA_MCS_DATATYPE_H_INCLUDED
#define HA_MCS_DATATYPE_H_INCLUDED

/*
  Interface classes for MariaDB data types (e.g. Field) for TypeHandler.
  These classes are needed to avoid TypeHandler dependency on
  MariaDB header files.
*/


namespace datatypes {

class StoreFieldMariaDB: public StoreField
{
  Field *m_field;
  const CalpontSystemCatalog::ColType &m_type;
public:
  StoreFieldMariaDB(Field *f, CalpontSystemCatalog::ColType &type)
   :m_field(f), m_type(type)
  { }
  const CalpontSystemCatalog::ColType &type() const { return m_type; }
  int32_t colWidth() const override { return m_type.colWidth; }
  int32_t precision() const override { return m_type.precision; }

  int store_date(int64_t val) override
  {
    char tmp[256];
    DataConvert::dateToString(val, tmp, sizeof(tmp)-1);
    return store_string(tmp, strlen(tmp));
  }

  int store_datetime(int64_t val) override
  {
    char tmp[256];
    DataConvert::datetimeToString(val, tmp, sizeof(tmp)-1, m_type.precision);
    return store_string(tmp, strlen(tmp));
  }

  int store_time(int64_t val) override
  {
    char tmp[256];
    DataConvert::timeToString(val, tmp, sizeof(tmp)-1, m_type.precision);
    return store_string(tmp, strlen(tmp));
  }

  int store_timestamp(int64_t val) override
  {
    char tmp[256];
    DataConvert::timestampToString(val, tmp, sizeof(tmp),
                                   current_thd->variables.time_zone->get_name()->ptr(),
                                   m_type.precision);
    return store_string(tmp, strlen(tmp));
  }

  int store_string(const char *str, size_t length) override
  {
    return m_field->store(str, length, m_field->charset());
  }
  int store_varbinary(const char *str, size_t length) override
  {
    if (get_varbin_always_hex(current_thd))
    {
      size_t ll = length * 2;
      boost::scoped_array<char> sca(new char[ll]);
      ConstString(str, length).bin2hex(sca.get());
      return m_field->store_binary(sca.get(), ll);
    }
    return m_field->store_binary(str, length);
  }

  int store_xlonglong(int64_t val) override
  {
    idbassert(dynamic_cast<Field_num*>(m_field));
    return m_field->store(val, static_cast<Field_num*>(m_field)->unsigned_flag);
  }

  int store_float(float dl) override
  {
    if (dl == std::numeric_limits<float>::infinity())
    {
      m_field->set_null();
      return 1;
    }

    // bug 3485, reserve enough space for the longest float value
    // -3.402823466E+38 to -1.175494351E-38, 0, and
    // 1.175494351E-38 to 3.402823466E+38.
    m_field->field_length = 40;
    return m_field->store(dl);
  }

  int store_double(double dl) override
  {
    if (dl == std::numeric_limits<double>::infinity())
    {
      m_field->set_null();
      return 1;
    }

    if (m_field->type() == MYSQL_TYPE_NEWDECIMAL)
    {
      char buf[310];
      // reserve enough space for the longest double value
      // -1.7976931348623157E+308 to -2.2250738585072014E-308, 0, and
      // 2.2250738585072014E-308 to 1.7976931348623157E+308.
      snprintf(buf, 310, "%.18g", dl);
      return m_field->store(buf, strlen(buf), m_field->charset());
    }

    // The server converts dl=-0 to dl=0 in (*f)->store().
    // This happens in the call to truncate_double().
    // This is an unexpected behaviour, so we directly store the
    // double value using the lower level float8store() function.
    // TODO Remove this when (*f)->store() handles this properly.
    m_field->field_length = 310;
    if (dl == 0)
    {
      float8store(m_field->ptr,dl);
      return 0;
    }
    return m_field->store(dl);
  }

  int store_long_double(long double dl) override
  {
    if (dl == std::numeric_limits<long double>::infinity())
    {
      m_field->set_null();
      return 1;
    }

    if (m_field->type() == MYSQL_TYPE_NEWDECIMAL)
    {
      char buf[310];
      snprintf(buf, 310, "%.20Lg", dl);
      return m_field->store(buf, strlen(buf), m_field->charset());
    }

    // reserve enough space for the longest double value
    // -1.7976931348623157E+308 to -2.2250738585072014E-308, 0, and
    // 2.2250738585072014E-308 to 1.7976931348623157E+308.
    m_field->field_length = 310;
    return m_field->store(static_cast<double>(dl));
  }

  int store_decimal64(int64_t val) override
  {
    // @bug4388 stick to InfiniDB's scale in case mysql gives wrong scale due
    // to create vtable limitation.
    //if (f2->dec < m_type.scale)
    //  f2->dec = m_type.scale;

    // WIP MCOL-641
    // This is too much
    char buf[256];
    dataconvert::DataConvert::decimalToString(val, (unsigned)m_type.scale,
                                              buf, sizeof(buf), m_type.colDataType);
    return m_field->store(buf, strlen(buf), m_field->charset());
  }

  int store_decimal128(const int128_t &val) override
  {
    // We won't have more than [+-][0][.] + up to 38 digits
    char buf[datatypes::Decimal::MAXLENGTH16BYTES];
    dataconvert::DataConvert::decimalToString((int128_t*) &val,
                                              (unsigned) m_type.scale,
                                              buf, (uint8_t) sizeof(buf),
                                              m_type.colDataType);
    return m_field->store(buf, strlen(buf), m_field->charset());
  }

  int store_lob(const char *str, size_t length) override
  {
    idbassert(dynamic_cast<Field_blob*>(m_field));
    Field_blob* f2 = static_cast<Field_blob*>(m_field);
    f2->set_ptr(length, (uchar*) str);
    return 0;
  }

};


/*******************************************************************************/

class WriteBatchFieldMariaDB: public WriteBatchField
{
public:
  Field *m_field;
  const CalpontSystemCatalog::ColType &m_type;
  WriteBatchFieldMariaDB(Field *field, const CalpontSystemCatalog::ColType type)
   :m_field(field), m_type(type)
  { }
  size_t ColWriteBatchDate(const uchar *buf, bool nullVal, ColBatchWriter &ci) override
  {
    // QQ: OLD DATE is not handled
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
    {
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    }
    else
    {
      const uchar* tmp1 = buf;
      uint32_t tmp = (tmp1[2] << 16) + (tmp1[1] << 8) + tmp1[0];
      int day = tmp & 0x0000001fl;
      int month = (tmp >> 5) & 0x0000000fl;
      int year = tmp >> 9;
      fprintf(ci.filePtr(), "%04d-%02d-%02d%c", year, month, day, ci.delimiter());
    }
    return 3;
  }
  size_t ColWriteBatchDatetime(const uchar *buf, bool nullVal, ColBatchWriter &ci) override
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
    {
      fprintf(ci.filePtr(), "%c", ci.delimiter());

      if (m_field->real_type() == MYSQL_TYPE_DATETIME2)
        return m_field->pack_length();
      else
        return 8;
    }

    if (m_field->real_type() == MYSQL_TYPE_DATETIME2)
    {
      // mariadb 10.1 compatibility -- MYSQL_TYPE_DATETIME2 introduced in mysql 5.6
      MYSQL_TIME ltime;
      longlong tmp = my_datetime_packed_from_binary(buf, m_field->decimals());
      TIME_from_longlong_datetime_packed(&ltime, tmp);

      if (!ltime.second_part)
      {
        fprintf(ci.filePtr(), "%04d-%02d-%02d %02d:%02d:%02d%c",
                ltime.year, ltime.month, ltime.day,
                ltime.hour, ltime.minute, ltime.second, ci.delimiter());
      }
      else
      {
        fprintf(ci.filePtr(), "%04d-%02d-%02d %02d:%02d:%02d.%ld%c",
                ltime.year, ltime.month, ltime.day,
                ltime.hour, ltime.minute, ltime.second,
                ltime.second_part, ci.delimiter());
      }

      return m_field->pack_length();
    }

    // Old DATETIME
    long long value = *((long long*) buf);
    long datePart = (long) (value / 1000000ll);
    int day = datePart % 100;
    int month = (datePart / 100) % 100;
    int year = datePart / 10000;
    fprintf(ci.filePtr(), "%04d-%02d-%02d ", year, month, day);

    long timePart = (long) (value - (long long) datePart * 1000000ll);
    int second = timePart % 100;
    int min = (timePart / 100) % 100;
    int hour = timePart / 10000;
    fprintf(ci.filePtr(), "%02d:%02d:%02d%c", hour, min, second, ci.delimiter());
    return 8;
  }
  size_t ColWriteBatchTime(const uchar *buf, bool nullVal, ColBatchWriter &ci) override
  {
    // QQ: why old TIME is not handled?
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
    {
      fprintf(ci.filePtr(), "%c", ci.delimiter());
      return m_field->pack_length();
    }

    MYSQL_TIME ltime;
    longlong tmp = my_time_packed_from_binary(buf, m_field->decimals());
    TIME_from_longlong_time_packed(&ltime, tmp);

    if (ltime.neg)
      fprintf(ci.filePtr(), "-");

    if (!ltime.second_part)
    {
      fprintf(ci.filePtr(), "%02d:%02d:%02d%c",
              ltime.hour, ltime.minute, ltime.second, ci.delimiter());
    }
    else
    {
      fprintf(ci.filePtr(), "%02d:%02d:%02d.%ld%c",
              ltime.hour, ltime.minute, ltime.second,
              ltime.second_part, ci.delimiter());
    }

    return m_field->pack_length();
  }

  size_t ColWriteBatchTimestamp(const uchar *buf, bool nullVal, ColBatchWriter &ci) override
  {
    // QQ: old TIMESTAMP is not handled

    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
    {
      fprintf(ci.filePtr(), "%c", ci.delimiter());
      return m_field->pack_length();
    }

    struct timeval tm;
    my_timestamp_from_binary(&tm, buf, m_field->decimals());

    MySQLTime time;
    gmtSecToMySQLTime(tm.tv_sec, time, current_thd->variables.time_zone->get_name()->ptr());

    if (!tm.tv_usec)
    {
      fprintf(ci.filePtr(), "%04d-%02d-%02d %02d:%02d:%02d%c",
              time.year, time.month, time.day,
              time.hour, time.minute, time.second, ci.delimiter());
    }
    else
    {
      fprintf(ci.filePtr(), "%04d-%02d-%02d %02d:%02d:%02d.%ld%c",
              time.year, time.month, time.day,
              time.hour, time.minute, time.second,
              tm.tv_usec, ci.delimiter());
    }
    return m_field->pack_length();
  }

  size_t ColWriteBatchChar(const uchar *buf, bool nullVal, ColBatchWriter &ci) override
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
    {
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    }
    else
    {
      if (current_thd->variables.sql_mode & MODE_PAD_CHAR_TO_FULL_LENGTH)
      {
        std::string escape;
        // Pad to the full length of the field
        if (ci.utf8())
          escape.assign((char*)buf, m_type.colWidth * 3);
        else
          escape.assign((char*)buf, m_type.colWidth);

        boost::replace_all(escape, "\\", "\\\\");

        fprintf(ci.filePtr(), "%c%.*s%c%c",
                ci.enclosed_by(),
                (int)escape.length(), escape.c_str(),
                ci.enclosed_by(), ci.delimiter());
      }
      else
      {
        std::string escape;
        // Get the actual data length
        bitmap_set_bit(m_field->table->read_set, m_field->field_index);
        String attribute;
        m_field->val_str(&attribute);

        escape.assign((char*)buf, attribute.length());
        boost::replace_all(escape, "\\", "\\\\");

        fprintf(ci.filePtr(), "%c%.*s%c%c",
                ci.enclosed_by(),
                (int)escape.length(), escape.c_str(),
                ci.enclosed_by(), ci.delimiter());
      }
    }

    if (ci.utf8())
      return m_type.colWidth * 3;
    else
      return m_type.colWidth;
  }

  size_t ColWriteBatchVarchar(const uchar *buf, bool nullVal, ColBatchWriter &ci) override
  {
    const uchar *buf0= buf;
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
    {
      fprintf(ci.filePtr(), "%c", ci.delimiter());

      if (!ci.utf8())
      {
        if (m_type.colWidth < 256)
        {
          buf++;
        }
        else
        {
          buf = buf + 2 ;
        }
      }
      else //utf8
      {
        if (m_type.colWidth < 86)
        {
          buf++;
        }
        else
        {
          buf = buf + 2 ;
        }
      }
    }
    else
    {
      int dataLength = 0;

      if (!ci.utf8())
      {
        if (m_type.colWidth < 256)
        {
          dataLength = *(uint8_t*) buf;
          buf++;
        }
        else
        {
          dataLength = *(uint16_t*) buf;
          buf = buf + 2 ;
        }
        std::string escape;
        escape.assign((char*)buf, dataLength);
        boost::replace_all(escape, "\\", "\\\\");
        fprintf(ci.filePtr(), "%c%.*s%c%c",
                ci.enclosed_by(),
                (int)escape.length(), escape.c_str(),
                ci.enclosed_by(), ci.delimiter());
      }
      else //utf8
      {
        if (m_type.colWidth < 86)
        {
          dataLength = *(uint8_t*) buf;
          buf++;
        }
        else
        {
          dataLength = *(uint16_t*) buf;
          buf = buf + 2 ;
        }

        std::string escape;
        escape.assign((char*)buf, dataLength);
        boost::replace_all(escape, "\\", "\\\\");

        fprintf(ci.filePtr(), "%c%.*s%c%c",
                ci.enclosed_by(),
                (int)escape.length(), escape.c_str(),
                ci.enclosed_by(), ci.delimiter());
      }
    }
    if (ci.utf8())
      buf += (m_type.colWidth * 3);
    else
      buf += m_type.colWidth;
    return buf - buf0;
  }

  size_t ColWriteBatchSInt64(const uchar *buf, bool nullVal, ColBatchWriter &ci) override
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    else
      fprintf(ci.filePtr(), "%lld%c", *((long long*)buf), ci.delimiter());
    return 8;
  }

  size_t ColWriteBatchUInt64(const uchar *buf, bool nullVal, ColBatchWriter &ci) override 
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    else
      fprintf(ci.filePtr(), "%llu%c", *((long long unsigned*)buf), ci.delimiter());
    return 8;
  }


  size_t ColWriteBatchSInt32(const uchar *buf, bool nullVal, ColBatchWriter &ci) override 
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    else
      fprintf(ci.filePtr(), "%d%c", *((int32_t*)buf), ci.delimiter());
    return 4;
  }

  size_t ColWriteBatchUInt32(const uchar *buf, bool nullVal, ColBatchWriter &ci) override 
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    else
      fprintf(ci.filePtr(), "%u%c", *((uint32_t*)buf), ci.delimiter());
    return 4;
  }

  size_t ColWriteBatchSInt16(const uchar *buf, bool nullVal, ColBatchWriter &ci) override 
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    else
      fprintf(ci.filePtr(), "%d%c", *((int16_t*)buf), ci.delimiter());
    return 2;
  }

  size_t ColWriteBatchUInt16(const uchar *buf, bool nullVal, ColBatchWriter &ci) override 
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    else
      fprintf(ci.filePtr(), "%u%c", *((uint16_t*)buf), ci.delimiter());
    return 2;
  }

  size_t ColWriteBatchSInt8(const uchar *buf, bool nullVal, ColBatchWriter &ci) override 
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    else
      fprintf(ci.filePtr(), "%d%c", *((int8_t*)buf), ci.delimiter());
    return 1;
  }

  size_t ColWriteBatchUInt8(const uchar *buf, bool nullVal, ColBatchWriter &ci) override 
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    else
      fprintf(ci.filePtr(), "%u%c", *((uint8_t*)buf), ci.delimiter());
    return 1;
  }


  size_t ColWriteBatchXFloat(const uchar *buf, bool nullVal, ColBatchWriter &ci) override 
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    else
    {
      float val = *((float*)buf);

      if ((fabs(val) > (1.0 / IDB_pow[4])) && (fabs(val) < (float) IDB_pow[6]))
      {
        fprintf(ci.filePtr(), "%.7f%c", val, ci.delimiter());
      }
      else
      {
        fprintf(ci.filePtr(), "%e%c", val, ci.delimiter());
      }

      //fprintf(ci.filePtr(), "%.7g|", *((float*)buf2));
      //printf("%.7f|", *((float*)buf2));
    }

    return 4;
  }


  size_t ColWriteBatchXDouble(const uchar *buf, bool nullVal, ColBatchWriter &ci) override 
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    else
    {
      fprintf(ci.filePtr(), "%.15g%c", *((double*)buf), ci.delimiter());
      //printf("%.15g|", *((double*)buf));
    }

    return 8;
  }


  size_t ColWriteBatchSLongDouble(const uchar *buf, bool nullVal, ColBatchWriter &ci) override 
  {
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
      fprintf(ci.filePtr(), "%c", ci.delimiter());
    else
      fprintf(ci.filePtr(), "%.15Lg%c", *((long double*)buf), ci.delimiter());
    return sizeof(long double);
  }


  size_t ColWriteBatchXDecimal(const uchar *buf, bool nullVal, ColBatchWriter &ci) override 
  {
    uint bytesBefore = 1;
    uint totalBytes = 9;

    switch (m_type.precision)
    {
      case 18:
      case 17:
      case 16:
      {
        totalBytes = 8;
        break;
      }

      case 15:
      case 14:
      {
        totalBytes = 7;
        break;
      }

      case 13:
      case 12:
      {
        totalBytes =  6;
        break;
      }

      case 11:
      {
        totalBytes =  5;
        break;
      }

      case 10:
      {
        totalBytes =  5;
        break;
      }

      case 9:
      case 8:
      case 7:
      {
        totalBytes =  4;
        break;
      }

      case 6:
      case 5:
      {
        totalBytes = 3;
        break;
      }

      case 4:
      case 3:
      {
        totalBytes =  2;
        break;
      }

      case 2:
      case 1:
      {
        totalBytes = 1;
        break;
      }

      default:
        break;
    }

    switch (m_type.scale)
    {
      case 0:
      {
        bytesBefore = totalBytes;
        break;
      }

      case 1: //1 byte for digits after decimal point
      {
        if ((m_type.precision != 16) && (m_type.precision != 14)
            && (m_type.precision != 12) && (m_type.precision != 10)
            && (m_type.precision != 7) && (m_type.precision != 5)
            && (m_type.precision != 3) && (m_type.precision != 1))
          totalBytes++;

        bytesBefore = totalBytes - 1;
        break;
      }

      case 2: //1 byte for digits after decimal point
      {
        if ((m_type.precision == 18) || (m_type.precision == 9))
          totalBytes++;

        bytesBefore = totalBytes - 1;
        break;
      }

      case 3: //2 bytes for digits after decimal point
      {
        if ((m_type.precision != 16) && (m_type.precision != 14)
            && (m_type.precision != 12) && (m_type.precision != 7)
            && (m_type.precision != 5) && (m_type.precision != 3))
          totalBytes++;

        bytesBefore = totalBytes - 2;
        break;
      }

      case 4:
      {
        if ((m_type.precision == 18) || (m_type.precision == 11)
            || (m_type.precision == 9))
          totalBytes++;

        bytesBefore = totalBytes - 2;
        break;

      }

      case 5:
      {
        if ((m_type.precision != 16) && (m_type.precision != 14)
            && (m_type.precision != 7) && (m_type.precision != 5))
          totalBytes++;

        bytesBefore = totalBytes - 3;
        break;
      }

      case 6:
      {
        if ((m_type.precision == 18) || (m_type.precision == 13)
            || (m_type.precision == 11) || (m_type.precision == 9))
          totalBytes++;

        bytesBefore = totalBytes - 3;
        break;
      }

      case 7:
      {
        if ((m_type.precision != 16) && (m_type.precision != 7))
          totalBytes++;

        bytesBefore = totalBytes - 4;
        break;
      }

      case 8:
      {
        if ((m_type.precision == 18) || (m_type.precision == 15)
            || (m_type.precision == 13) || (m_type.precision == 11)
            || (m_type.precision == 9))
          totalBytes++;

        bytesBefore = totalBytes - 4;;
        break;
      }

      case 9:
      {
        bytesBefore = totalBytes - 4;;
        break;
      }

      case 10:
      {
        if ((m_type.precision != 16) && (m_type.precision != 14)
            && (m_type.precision != 12) && (m_type.precision != 10))
          totalBytes++;

        bytesBefore = totalBytes - 5;;
        break;
      }

      case 11:
      {
        if (m_type.precision == 18)
          totalBytes++;

        bytesBefore = totalBytes - 5;
        break;
      }

      case 12:
      {
        if ((m_type.precision != 16) && (m_type.precision != 14)
            && (m_type.precision != 12))
          totalBytes++;

        bytesBefore = totalBytes - 6;
        break;
      }

      case 13:
      {
        if (m_type.precision == 18)
          totalBytes++;

        bytesBefore = totalBytes - 6;
        break;
      }

      case 14:
      {
        if ((m_type.precision != 16) && (m_type.precision != 14))
          totalBytes++;

        bytesBefore = totalBytes - 7;
        break;
      }

      case 15:
      {
        if (m_type.precision == 18)
          totalBytes++;

        bytesBefore = totalBytes - 7;
        break;
      }

      case 16:
      {
        if (m_type.precision != 16)
          totalBytes++;

        bytesBefore = totalBytes - 8;
        break;
      }

      case 17:
      {
        if (m_type.precision == 18)
          totalBytes++;

        bytesBefore = totalBytes - 8;
        break;
      }

      case 18:
      {
        bytesBefore = totalBytes - 8;
        break;
      }

      default:
        break;
    }

    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
    {
      fprintf(ci.filePtr(), "%c", ci.delimiter());
      //printf("|");
    }
    else
    {
      uint32_t mask [5] = {0, 0xFF, 0xFFFF, 0xFFFFFF, 0xFFFFFFFF};
      char neg = '-';

      if (m_type.scale == 0)
      {
        const uchar* tmpBuf = buf;
        //test flag bit for sign
        bool posNum  = tmpBuf[0] & (0x80);
        uchar tmpChr = tmpBuf[0];
        tmpChr ^= 0x80; //flip the bit
        int32_t tmp1 = tmpChr;

        if (totalBytes > 4)
        {
          for (uint i = 1; i < (totalBytes - 4); i++)
          {
            tmp1 = (tmp1 << 8) + tmpBuf[i];
          }

          if (( tmp1 != 0 ) && (tmp1 != -1))
          {
            if (!posNum)
            {
              tmp1 = mask[totalBytes - 4] - tmp1;

              if (tmp1 != 0)
              {
                fprintf(ci.filePtr(), "%c", neg);
                //printf("%c", neg);
              }
            }

            if (tmp1 != 0)
            {
              fprintf(ci.filePtr(), "%d", tmp1);
              ////printf("%d", tmp1);
            }
          }

          int32_t tmp2 = tmpBuf[totalBytes - 4];

          for (uint i = (totalBytes - 3); i < totalBytes; i++)
          {
            tmp2 = (tmp2 << 8) + tmpBuf[i];
          }

          if ( tmp1 != 0 )
          {
            if (!posNum)
            {
              tmp2 = mask[4] - tmp2;

              if (tmp1 == -1)
              {
                fprintf(ci.filePtr(), "%c", neg);
                fprintf(ci.filePtr(), "%d%c", tmp2, ci.delimiter());
                ////printf("%c", neg);
                //////printf( "%d|", tmp2);
              }
              else
              {
                fprintf(ci.filePtr(), "%09u%c", tmp2, ci.delimiter());
                ////printf("%09u|", tmp2);
              }
            }
            else
            {
              fprintf(ci.filePtr(), "%09u%c", tmp2, ci.delimiter());
              //printf("%09u|", tmp2);
            }
          }
          else
          {
            if (!posNum)
            {
              tmp2 = mask[4] - tmp2;
              fprintf(ci.filePtr(), "%c", neg);
              //printf("%c", neg);
            }

            fprintf(ci.filePtr(), "%d%c", tmp2, ci.delimiter());
            //printf("%d|", tmp2);
          }
        }
        else
        {
          for (uint i = 1; i < totalBytes; i++)
          {
            tmp1 = (tmp1 << 8) + tmpBuf[i];
          }

          if (!posNum)
          {
            tmp1 = mask[totalBytes] - tmp1;
            fprintf(ci.filePtr(), "%c", neg);
            //printf("%c", neg);
          }

          fprintf(ci.filePtr(), "%d%c", tmp1, ci.delimiter());
          //printf("%d|", tmp1);
        }
      }
      else
      {
        const uchar* tmpBuf = buf;
        //test flag bit for sign
        bool posNum  = tmpBuf[0] & (0x80);
        uchar tmpChr = tmpBuf[0];
        tmpChr ^= 0x80; //flip the bit
        int32_t tmp1 = tmpChr;

        //fetch the digits before decimal point
        if (bytesBefore == 0)
        {
          if (!posNum)
          {
            fprintf(ci.filePtr(), "%c", neg);
            //printf("%c", neg);
          }

          fprintf(ci.filePtr(), "0.");
          //printf("0.");
        }
        else if (bytesBefore > 4)
        {
          for (uint i = 1; i < (bytesBefore - 4); i++)
          {
            tmp1 = (tmp1 << 8) + tmpBuf[i];
          }

          if (!posNum)
          {
            tmp1 = mask[bytesBefore - 4] - tmp1;
          }

          if (( tmp1 != 0 ) && (tmp1 != -1))
          {
            if (!posNum)
            {
              fprintf(ci.filePtr(), "%c", neg);
              //printf("%c", neg);
            }

            fprintf(ci.filePtr(), "%d", tmp1);
            //printf("%d", tmp1);
          }

          tmpBuf += (bytesBefore - 4);
          int32_t tmp2 = *((int32_t*)tmpBuf);
          tmp2 = ntohl(tmp2);

          if ( tmp1 != 0 )
          {
            if (!posNum)
            {
              tmp2 = mask[4] - tmp2;
            }

            if (tmp1 == -1)
            {
              fprintf(ci.filePtr(), "%c", neg);
              fprintf(ci.filePtr(), "%d.", tmp2);
              //printf("%c", neg);
              //printf("%d.", tmp2);
            }
            else
            {
              fprintf(ci.filePtr(), "%09u.", tmp2);
              //printf("%09u.", tmp2);
            }
          }
          else
          {
            if (!posNum)
            {
              tmp2 = mask[4] - tmp2;
              fprintf(ci.filePtr(), "%c", neg);
              //printf("%c", neg);
            }

            fprintf(ci.filePtr(), "%d.", tmp2);
            //printf("%d.", tmp2);
          }
        }
        else
        {
          for (uint i = 1; i < bytesBefore; i++)
          {
            tmp1 = (tmp1 << 8) + tmpBuf[i];
          }

          if (!posNum)
          {
            tmp1 = mask[bytesBefore] - tmp1;
            fprintf(ci.filePtr(), "%c", neg);
            //printf("%c", neg);
          }

          fprintf(ci.filePtr(), "%d.", tmp1);
          //printf("%d.", tmp1);
        }

        //fetch the digits after decimal point
        int32_t tmp2 = 0;

        if (bytesBefore > 4)
          tmpBuf += 4;
        else
          tmpBuf += bytesBefore;

        tmp2 = tmpBuf[0];

        if ((totalBytes - bytesBefore) < 5)
        {
          for (uint j = 1; j < (totalBytes - bytesBefore); j++)
          {
            tmp2 = (tmp2 << 8) + tmpBuf[j];
          }

          int8_t digits = m_type.scale - 9; //9 digits is a 4 bytes chunk

          if ( digits <= 0 )
            digits = m_type.scale;

          if (!posNum)
          {
            tmp2 = mask[totalBytes - bytesBefore] - tmp2;
          }

          fprintf(ci.filePtr(), "%0*u%c", digits, tmp2, ci.delimiter());
          //printf("%0*u|", digits, tmp2);
        }
        else
        {
          for (uint j = 1; j < 4; j++)
          {
            tmp2 = (tmp2 << 8) +  tmpBuf[j];
          }

          if (!posNum)
          {
            tmp2 = mask[4] - tmp2;
          }

          fprintf(ci.filePtr(), "%09u", tmp2);
          //printf("%09u", tmp2);

          tmpBuf += 4;
          int32_t tmp3 = tmpBuf[0];

          for (uint j = 1; j < (totalBytes - bytesBefore - 4); j++)
          {
            tmp3 = (tmp3 << 8) +  tmpBuf[j];
          }

          int8_t digits = m_type.scale - 9; //9 digits is a 4 bytes chunk

          if ( digits < 0 )
            digits = m_type.scale;

          if (!posNum)
          {
            tmp3 = mask[totalBytes - bytesBefore - 4] - tmp3;
          }

          fprintf(ci.filePtr(), "%0*u%c", digits, tmp3, ci.delimiter());
          //printf("%0*u|", digits, tmp3);
        }
      }
    }

    return totalBytes;
  }


  size_t ColWriteBatchVarbinary(const uchar *buf0, bool nullVal, ColBatchWriter &ci) override 
  {
    const uchar *buf= buf0;
    if (nullVal && (m_type.constraintType != CalpontSystemCatalog::NOTNULL_CONSTRAINT))
    {
      fprintf(ci.filePtr(), "%c", ci.delimiter());

      if (!ci.utf8())
      {
        if (m_type.colWidth < 256)
        {
          buf++;
        }
        else
        {
          buf = buf + 2;
        }
      }
      else //utf8
      {
        if (m_type.colWidth < 86)
        {
          buf++;
        }
        else
        {
          buf = buf + 2 ;
        }
      }
    }
    else
    {
      int dataLength = 0;

      if (!ci.utf8())
      {
        if (m_type.colWidth < 256)
        {
          dataLength = *(int8_t*) buf;
          buf++;
        }
        else
        {
          dataLength = *(int16_t*) buf;
          buf = buf + 2 ;
        }

        const uchar* tmpBuf = buf;

        for (int32_t i = 0; i < dataLength; i++)
        {
          fprintf(ci.filePtr(), "%02x", *(uint8_t*)tmpBuf);
          tmpBuf++;
        }

        fprintf(ci.filePtr(), "%c", ci.delimiter());
      }
      else //utf8
      {
        if (m_type.colWidth < 86)
        {
          dataLength = *(int8_t*) buf;
          buf++;
        }
        else
        {
          dataLength = *(uint16_t*) buf;
          buf = buf + 2 ;
        }

        if ( dataLength > m_type.colWidth)
          dataLength = m_type.colWidth;

        const uchar* tmpBuf = buf;

        for (int32_t i = 0; i < dataLength; i++)
        {
          fprintf(ci.filePtr(), "%02x", *(uint8_t*)tmpBuf);
          tmpBuf++;
        }

        fprintf(ci.filePtr(), "%c", ci.delimiter());
      }
    }

    if (ci.utf8())
      buf += (m_type.colWidth * 3); // QQ: why? It is varbinary!
    else
      buf += m_type.colWidth;

    return buf - buf0;
  }


  size_t ColWriteBatchBlob(const uchar *buf0, bool nullVal, ColBatchWriter &ci) override 
  {
    const uchar *buf= buf0;
    // MCOL-4005 Note that we don't handle nulls as a special
    // case here as we do for other datatypes, the below works
    // as expected for nulls.
    uint32_t dataLength = 0;
    uintptr_t* dataptr;
    uchar* ucharptr;
    uint colWidthInBytes = (ci.utf8() ?
      m_type.colWidth * 3: m_type.colWidth);

    if (colWidthInBytes < 256)
    {
      dataLength = *(uint8_t*) buf;
      buf++;
    }
    else if (colWidthInBytes < 65536)
    {
      dataLength = *(uint16_t*) buf;
      buf += 2;
    }
    else if (colWidthInBytes < 16777216)
    {
      dataLength = *(uint16_t*) buf;
      dataLength |= ((int) buf[2]) << 16;
      buf += 3;
    }
    else
    {
      dataLength = *(uint32_t*) buf;
      buf += 4;
    }

    // buf contains pointer to blob, for example:
    // (gdb) p (char*)*(uintptr_t*)buf
    // $43 = 0x7f68500c58f8 "hello world"

    dataptr = (uintptr_t*)buf;
    ucharptr = (uchar*)*dataptr;
    buf += sizeof(uintptr_t);

    if (m_type.colDataType == CalpontSystemCatalog::BLOB)
    {
      for (uint32_t i = 0; i < dataLength; i++)
      {
        fprintf(ci.filePtr(), "%02x", *(uint8_t*)ucharptr);
        ucharptr++;
      }

      fprintf(ci.filePtr(), "%c", ci.delimiter());
    }
    else
    {
      // TEXT Column
      std::string escape;
      escape.assign((char*)ucharptr, dataLength);
      boost::replace_all(escape, "\\", "\\\\");
      fprintf(ci.filePtr(), "%c%.*s%c%c",
              ci.enclosed_by(),
              (int)escape.length(), escape.c_str(),
              ci.enclosed_by(), ci.delimiter());
    }

    return buf - buf0;
  }

};

} // end of namespace datatypes

#endif

// vim:ts=2 sw=2:
