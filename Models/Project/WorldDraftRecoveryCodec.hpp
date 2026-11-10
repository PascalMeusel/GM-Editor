#pragma once
#include "Models/Project/WorldDraftRecovery.hpp"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>

namespace WorldDraftRecovery::detail
{
inline void check(bool value,const char *message)
{if(!value)throw std::runtime_error(message);}
inline void identity(const std::string &value,bool required=false)
{check(value.size()<=MaxIdentityBytes&&(!required||!value.empty()),"Invalid World recovery identity.");}
struct Budget
{
    std::size_t parents=0,children=0,points=0;
    std::set<EntityId> ids;
    void consume(std::size_t n,std::size_t &used,std::size_t limit)
    {check(n<=limit-used,"World recovery record limit exceeded.");used+=n;}
    void parent(const EntityId &key,const EntityId &base,const EntityId &edited)
    {identity(key,true);check(key==base&&key==edited&&ids.insert(key).second,"Duplicate or mismatched World draft identity.");}
};
struct Writer : Budget
{
    static constexpr bool Reading=false;
    std::string bytes;
    void token(std::string_view value)
    {
        const auto prefix=std::to_string(value.size())+":";
        check(value.size()<=MaxTextBytes&&prefix.size()+value.size()<=MaxPayloadBytes-bytes.size(),"World recovery size limit exceeded.");
        bytes+=prefix;bytes.append(value.data(),value.size());
    }
    void text(const std::string &v){token(v);}
    void id(const std::string &v){identity(v);token(v);}
    void integer(int v){token(std::to_string(v));}
    void flag(bool v){token(v?"1":"0");}
    void number(double v)
    {
        check(std::isfinite(v),"Nonfinite World draft number cannot be checkpointed.");
        std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(std::numeric_limits<double>::max_digits10)<<v;token(s.str());
    }
    template<class T> void enumeration(T v,int maximum)
    {const auto n=static_cast<int>(v);check(n>=0&&n<=maximum,"Invalid World recovery enum.");integer(n);}
    std::size_t count(std::size_t n,std::size_t &used,std::size_t limit)
    {consume(n,used,limit);token(std::to_string(n));return n;}
};
struct Reader : Budget
{
    static constexpr bool Reading=true;
    std::string_view bytes;
    std::size_t offset=0;
    explicit Reader(std::string_view value):bytes(value)
    {check(value.size()<=MaxPayloadBytes,"World recovery payload is too large.");}
    std::string_view token()
    {
        const auto end=bytes.find(':',offset);
        check(end!=std::string_view::npos&&end>offset&&end-offset<=8,"Invalid World recovery length.");
        std::size_t n=0;const auto prefix=bytes.substr(offset,end-offset);
        const auto parsed=std::from_chars(prefix.data(),prefix.data()+prefix.size(),n);
        check(parsed.ec==std::errc{}&&parsed.ptr==prefix.data()+prefix.size(),"Invalid World recovery length.");
        offset=end+1;check(n<=MaxTextBytes&&n<=bytes.size()-offset,"Truncated or oversized World recovery field.");
        const auto result=bytes.substr(offset,n);offset+=n;return result;
    }
    void text(std::string &v){const auto s=token();v.assign(s.data(),s.size());}
    void id(std::string &v){const auto s=token();check(s.size()<=MaxIdentityBytes,"Invalid World recovery identity.");v.assign(s.data(),s.size());}
    void integer(int &v)
    {const auto s=token();const auto p=std::from_chars(s.data(),s.data()+s.size(),v);
     check(!s.empty()&&p.ec==std::errc{}&&p.ptr==s.data()+s.size(),"Invalid World recovery integer.");}
    void flag(bool &v){const auto s=token();check(s=="0"||s=="1","Invalid World recovery boolean.");v=s=="1";}
    void number(double &v)
    {
        const auto s=token();check(!s.empty()&&s.size()<=64,"Invalid World recovery number.");
        const auto p=std::from_chars(s.data(),s.data()+s.size(),v,std::chars_format::general);
        check(p.ec==std::errc{}&&p.ptr==s.data()+s.size()&&std::isfinite(v),"Invalid World recovery number.");
    }
    template<class T> void enumeration(T &v,int maximum)
    {int n=0;integer(n);check(n>=0&&n<=maximum,"Invalid World recovery enum.");v=static_cast<T>(n);}
    std::size_t count(std::size_t,std::size_t &used,std::size_t limit)
    {
        const auto s=token();std::size_t n=0;const auto p=std::from_chars(s.data(),s.data()+s.size(),n);
        check(!s.empty()&&p.ec==std::errc{}&&p.ptr==s.data()+s.size(),"Invalid World recovery count.");
        consume(n,used,limit);
        check(n<=(bytes.size()-offset)/2,"Truncated World recovery record list.");return n;
    }
};
}
