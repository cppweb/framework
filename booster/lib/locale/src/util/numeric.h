//
//  Copyright (c) 2009-2010 Artyom Beilis (Tonkikh)
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#ifndef BOOSTER_LOCALE_IMPL_UTIL_NUMERIC_HPP
#define BOOSTER_LOCALE_IMPL_UTIL_NUMERIC_HPP
#include <locale>
#include <string>
#include <ios>
#include <booster/locale/formatting.h>
#include <sstream>
#include <stdlib.h>

namespace booster {
namespace locale {
namespace util {

template<typename CharType>
class base_num_format : public std::num_put<CharType>
{
public:
    typedef typename std::num_put<CharType>::iter_type iter_type;
    typedef std::basic_string<CharType> string_type;
    typedef CharType char_type;

    base_num_format(size_t refs = 0) : 
        std::num_put<CharType>(refs)
    {
    }
protected: 
    

    virtual iter_type do_put (iter_type out, std::ios_base &ios, char_type fill, long val) const
    {
        return do_real_put(out,ios,fill,val);
    }
    virtual iter_type do_put (iter_type out, std::ios_base &ios, char_type fill, unsigned long val) const
    {
        return do_real_put(out,ios,fill,val);
    }
    virtual iter_type do_put (iter_type out, std::ios_base &ios, char_type fill, double val) const
    {
        return do_real_put(out,ios,fill,val);
    }
    virtual iter_type do_put (iter_type out, std::ios_base &ios, char_type fill, long double val) const
    {
        return do_real_put(out,ios,fill,val);
    }
    
    #ifndef BOOSTER_NO_LONG_LONG 
    virtual iter_type do_put (iter_type out, std::ios_base &ios, char_type fill, long long val) const
    {
        return do_real_put(out,ios,fill,val);
    }
    virtual iter_type do_put (iter_type out, std::ios_base &ios, char_type fill, unsigned long long val) const
    {
        return do_real_put(out,ios,fill,val);
    }
    #endif


private:



    template<typename ValueType>
    iter_type do_real_put (iter_type out, std::ios_base &ios, char_type fill, ValueType val) const
    {
        typedef std::num_put<char_type> super;

        ios_info &info=ios_info::get(ios);

        switch(info.display_flags()) {
        case flags::posix:
            {
                typedef std::basic_ostringstream<char_type> sstream_type;
                sstream_type ss;
                ss.imbue(std::locale::classic());
                ss.flags(ios.flags());
                ss.precision(ios.precision());
                ss.width(ios.width());
                iter_type ret_ptr = super::do_put(out,ss,fill,val);
                ios.width(0);
                return ret_ptr;
            }
        case flags::date:
            return format_time(out,ios,fill,static_cast<time_t>(val),'x');
        case flags::time:
            return format_time(out,ios,fill,static_cast<time_t>(val),'X');
        case flags::datetime:
            return format_time(out,ios,fill,static_cast<time_t>(val),'c');
        case flags::strftime:
            return format_time(out,ios,fill,static_cast<time_t>(val),info.date_time_pattern<char_type>());
        case flags::currency:
            {
                bool nat =  info.currency_flags()==flags::currency_default 
                            || info.currency_flags() == flags::currency_national;
                bool intl = !nat;
                return do_format_currency(intl,out,ios,fill,static_cast<long double>(val));
            }

        case flags::number:
        case flags::percent:
        case flags::spellout:
        case flags::ordinal:
        default:
            return super::do_put(out,ios,fill,val);
        }
    }

    virtual iter_type do_format_currency(bool intl,iter_type out,std::ios_base &ios,char_type fill,long double val) const
    {
        if(intl)
            return format_currency<true>(out,ios,fill,val);
        else
            return format_currency<false>(out,ios,fill,val);
    }

    template<bool intl>
    iter_type format_currency(iter_type out,std::ios_base &ios,char_type fill,long double val) const
    {
        std::locale loc = ios.getloc();
        int digits = std::use_facet<std::moneypunct<char_type,intl> >(loc).frac_digits();
        while(digits > 0) {
            val*=10;
            digits --;
        }
        std::ios_base::fmtflags f=ios.flags();
        ios.flags(f | std::ios_base::showbase);
        out = std::use_facet<std::money_put<char_type> >(loc).put(out,intl,ios,fill,val);
        ios.flags(f);
        return out;
    }

    iter_type format_time(iter_type out,std::ios_base &ios,char_type fill,time_t time,char c) const
    {
        string_type fmt;
        fmt+=char_type('%');
        fmt+=char_type(c);
        return format_time(out,ios,fill,time,fmt);
    }

    iter_type format_time(iter_type out,std::ios_base &ios,char_type fill,time_t time,string_type const &format) const
    {
        std::string tz = ios_info::get(ios).time_zone();
        std::tm tm;
        #if defined(__linux) || defined(__FreeBSD__) || defined(__APPLE__) 
        std::vector<char> tmp_buf(tz.c_str(),tz.c_str()+tz.size()+1);
        #endif
        if(tz.empty()) {
            #ifdef BOOSTER_WIN_NATIVE
            /// Windows uses TLS
            tm = *localtime(&time);
            #else
            localtime_r(&time,&tm);
            #endif
        }
        else  {
            int gmtoff = parse_tz(tz);
            time+=gmtoff;
            #ifdef BOOSTER_WIN_NATIVE
            /// Windows uses TLS
            tm = *gmtime(&time);
            #else
            gmtime_r(&time,&tm);
            #endif
            
            #if defined(__linux) || defined(__FreeBSD__) || defined(__APPLE__) 
            // These have extra fields to specify timezone
            if(gmtoff!=0) {
                // bsd and apple want tm_zone be non-const
                tm.tm_zone=&tmp_buf.front();
                tm.tm_gmtoff = gmtoff;
            }
            #endif
        }
        return std::use_facet<std::time_put<char_type> >(ios.getloc()).put(out,ios,fill,&tm,format.c_str(),format.c_str()+format.size());
    }

    int parse_tz(std::string const &tz) const
    {
        int gmtoff = 0;
        std::string ltz;
        for(unsigned i=0;i<tz.size();i++) {
            if('a' <= tz[i] && tz[i] <= 'z')
                ltz += tz[i]-'a' + 'A';
            else if(tz[i]==' ')
                ;
            else
                ltz+=tz[i];
        }
        if(ltz.compare(0,3,"GMT")!=0 && ltz.compare(0,3,"UTC")!=0)
            return 0;
        if(ltz.size()<=3)
            return 0;
        char const *begin = ltz.c_str()+3;
        char *end=0;
        int hours = strtol(begin,&end,10);
        if(end != begin) {
            gmtoff+=hours * 3600;
        }
        if(*end==':') {
            begin=end+1;
            int minutes = strtol(begin,&end,10);
            if(end!=begin)
                gmtoff+=minutes * 60;
        }
        return gmtoff;
    }


};  /// num_format


template<typename CharType>
class base_num_parse : public std::num_get<CharType>
{
public:
    base_num_parse(size_t refs = 0) : 
        std::num_get<CharType>(refs)
    {
    }
protected: 
    typedef typename std::num_get<CharType>::iter_type iter_type;
    typedef std::basic_string<CharType> string_type;
    typedef CharType char_type;

    virtual iter_type do_get(iter_type in, iter_type end, std::ios_base &ios,std::ios_base::iostate &err,long &val) const
    {
        return do_real_get(in,end,ios,err,val);
    }

    virtual iter_type do_get(iter_type in, iter_type end, std::ios_base &ios,std::ios_base::iostate &err,unsigned short &val) const
    {
        return do_real_get(in,end,ios,err,val);
    }

    virtual iter_type do_get(iter_type in, iter_type end, std::ios_base &ios,std::ios_base::iostate &err,unsigned int &val) const
    {
        return do_real_get(in,end,ios,err,val);
    }

    virtual iter_type do_get(iter_type in, iter_type end, std::ios_base &ios,std::ios_base::iostate &err,unsigned long &val) const
    {
        return do_real_get(in,end,ios,err,val);
    }

    virtual iter_type do_get(iter_type in, iter_type end, std::ios_base &ios,std::ios_base::iostate &err,float &val) const
    {
        return do_real_get(in,end,ios,err,val);
    }

    virtual iter_type do_get(iter_type in, iter_type end, std::ios_base &ios,std::ios_base::iostate &err,double &val) const
    {
        return do_real_get(in,end,ios,err,val);
    }

    virtual iter_type do_get (iter_type in, iter_type end, std::ios_base &ios,std::ios_base::iostate &err,long double &val) const
    {
        return do_real_get(in,end,ios,err,val);
    }

    #ifndef BOOSTER_NO_LONG_LONG 
    virtual iter_type do_get (iter_type in, iter_type end, std::ios_base &ios,std::ios_base::iostate &err,long long &val) const
    {
        return do_real_get(in,end,ios,err,val);
    }

    virtual iter_type do_get (iter_type in, iter_type end, std::ios_base &ios,std::ios_base::iostate &err,unsigned long long &val) const
    {
        return do_real_get(in,end,ios,err,val);
    }

    #endif

private:
    
    template<typename ValueType>
    iter_type do_real_get(iter_type in,iter_type end,std::ios_base &ios,std::ios_base::iostate &err,ValueType &val) const
    {
        typedef std::num_get<char_type> super;

        ios_info &info=ios_info::get(ios);

        switch(info.display_flags()) {
        case flags::posix:
            {
                std::stringstream ss;
                ss.imbue(std::locale::classic());
                ss.flags(ios.flags());
                ss.precision(ios.precision());
                return super::do_get(in,end,ss,err,val);
            }
        case flags::currency:
            {
                long double ret_val = 0;
                if(info.currency_flags()==flags::currency_default || info.currency_flags() == flags::currency_national)
                    in = parse_currency<false>(in,end,ios,err,ret_val);
                else
                    in = parse_currency<true>(in,end,ios,err,ret_val);
                if(!(err & std::ios_base::failbit))
                    val = static_cast<ValueType>(ret_val);
                return in;
            }

        // date-time parsing is not supported
        // due to buggy standard
        case flags::date:
        case flags::time:
        case flags::datetime:
        case flags::strftime:

        case flags::number:
        case flags::percent:
        case flags::spellout:
        case flags::ordinal:
        default:
            return super::do_get(in,end,ios,err,val);
        }
    }

    template<bool intl>
    iter_type parse_currency(iter_type in,iter_type end,std::ios_base &ios,std::ios_base::iostate &err,long double &val) const
    {
        std::locale loc = ios.getloc();
        int digits = std::use_facet<std::moneypunct<char_type,intl> >(loc).frac_digits();
        long double rval;
        in = std::use_facet<std::money_get<char_type> >(loc).get(in,end,intl,ios,err,rval);
        if(!(err & std::ios::failbit)) {
            while(digits > 0) {
                rval/=10;
                digits --;
            }
            val = rval;
        }
        return in;
    }


};

} // util
} // locale 
} //boost




#endif
// vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4
