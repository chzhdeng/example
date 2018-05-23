#include <stdio.h>
#include <string.h>
#include <wchar.h>

void GetUtf8(unsigned short uni)
{
    unsigned char utf_8[3];
    utf_8[0]=(0xe<<4) | ((uni & 0xf000)>>12);//相当于把二进制1110右移12位
    utf_8[1]=(0x2<<6) | ((uni & 0x0fc0)>>6); //相当于把二进制10右移16位
    utf_8[2]=(0x2<<6) | (uni & 0x003f);      //相当于取二进制低6位
    printf("UTF-8编码的第一个字节为0x%.2x  十进制是：%d  八进制是：%o\n",utf_8[0],utf_8[0],utf_8[0]);
    printf("UTF-8编码的第二个字节为0x%.2x  十进制是：%d  八进制是：%o\n",utf_8[1],utf_8[1],utf_8[1]);
    printf("UTF-8编码的第三个字节为0x%.2x  十进制是：%d  八进制是：%o\n",utf_8[2],utf_8[2],utf_8[2]);
}

void main()
{
    wchar_t str=L'邓';
    printf("'邓'字Unicode编码为%.4x\n",(unsigned short)str);
    GetUtf8((unsigned short)str);
}
