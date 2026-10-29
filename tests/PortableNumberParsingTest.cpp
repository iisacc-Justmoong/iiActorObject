#include <iiActorObject/Object3DNumberParsing.h>
#include <cmath>
#include <iostream>
#include <limits>
#include <locale>
#include <string_view>

template<class T> concept HasFloatingFromChars = requires(const char *a,const char *b,T &value)
{std::from_chars(a,b,value,std::chars_format::general);};

int main()
{
    using iiActorObject::object3d_detail::readNumber;
    struct Comma final : std::numpunct<char>{char do_decimal_point() const override{return ',';}};
    std::locale::global(std::locale(std::locale(),new Comma));
    double value=0;
    if(!readNumber("-.5e-1",value)||std::abs(value+.05)>1e-15)return 1;
    if(!readNumber("5e-324",value)||value!=std::numeric_limits<double>::denorm_min())return 2;
    if(!readNumber("0e-999",value)||value!=0)return 3;
    for(const std::string_view invalid:{"", "+1", ".", "nan", "inf", "1,5", "1e", "1e400", "1e-999", "0x1", " 1", "1x"})
        if(readNumber(invalid,value))return 4;
    int integer=0;if(!readNumber("-42",integer)||integer!=-42||readNumber("42x",integer))return 5;
    std::cout<<"portable number parsing passed; floating from_chars="<<HasFloatingFromChars<double><<'\n';
}
