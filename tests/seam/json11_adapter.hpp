#pragma once
// TEST-ONLY nlohmann-shaped value tree. Exercises production persistence branches;
// NOT the nlohmann parser, disk format implementation, SKSE serialization or engine ABI.
#include <variant>
#include <type_traits>
#include <array>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>
namespace nlohmann {
class json {
 using object=std::map<std::string,json>;using list=std::vector<json>;
 std::variant<std::monostate,bool,std::int64_t,std::uint64_t,double,std::string,list,object> v;
public:
 json()=default;json(const char* s):v(std::string(s)){}json(std::string s):v(std::move(s)){}json(bool b):v(b){}
 template<class T> requires(std::is_integral_v<T>&&!std::is_same_v<T,bool>)json(T n){if constexpr(std::is_signed_v<T>)v=static_cast<std::int64_t>(n);else v=static_cast<std::uint64_t>(n);}
 template<class T> requires(std::is_floating_point_v<T>)json(T n):v(static_cast<double>(n)){}
 template<class T,std::size_t N>json(const std::array<T,N>& a){list l;for(const auto& x:a)l.emplace_back(x);v=std::move(l);}
 json(std::initializer_list<json> values){
  bool obj=values.size()>0;for(const auto& x:values)if(!std::holds_alternative<list>(x.v)||std::get<list>(x.v).size()!=2||!std::holds_alternative<std::string>(std::get<list>(x.v)[0].v))obj=false;
  if(obj){object o;for(const auto& x:values){const auto& l=std::get<list>(x.v);o.emplace(std::get<std::string>(l[0].v),l[1]);}v=std::move(o);}else v=list(values);
 }
 static json array(){json j;j.v=list{};return j;}
 static json parse(std::istream&){throw std::runtime_error("TEST adapter does not parse JSON");}
 bool contains(const char* s)const{auto* o=std::get_if<object>(&v);return o&&o->contains(s);}
 json& operator[](const char* s){if(std::holds_alternative<std::monostate>(v))v=object{};return std::get<object>(v)[s];}
 const json& at(const char* s)const{return std::get<object>(v).at(s);}const json& at(int i)const{return std::get<list>(v).at(static_cast<std::size_t>(i));}
 json& at(const char* s){return std::get<object>(v).at(s);}json& at(int i){return std::get<list>(v).at(static_cast<std::size_t>(i));}
 void push_back(json j){std::get<list>(v).push_back(std::move(j));}auto begin()const{return std::get<list>(v).begin();}auto end()const{return std::get<list>(v).end();}
 std::size_t size()const{if(auto* l=std::get_if<list>(&v))return l->size();if(auto* o=std::get_if<object>(&v))return o->size();return 0;}
 template<class T>T get()const{
  if constexpr(std::is_same_v<T,std::string>)return std::get<std::string>(v);
  else if constexpr(std::is_same_v<T,bool>)return std::get<bool>(v);
  else if constexpr(std::is_arithmetic_v<T>){if(auto* p=std::get_if<std::int64_t>(&v))return static_cast<T>(*p);if(auto* p=std::get_if<std::uint64_t>(&v))return static_cast<T>(*p);if(auto* p=std::get_if<double>(&v))return static_cast<T>(*p);throw std::runtime_error("not numeric");}
  else {T out{};const auto& l=std::get<list>(v);if(l.size()!=out.size())throw std::runtime_error("array length");for(std::size_t i=0;i<out.size();++i)out[i]=l[i].template get<typename T::value_type>();return out;}
 }
 template<class T>T value(const char* k,T fallback)const{return contains(k)?at(k).template get<T>():fallback;}
};
}
