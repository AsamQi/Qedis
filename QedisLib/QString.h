#ifndef BERT_QSTRING_H
#define BERT_QSTRING_H

#include <string>

typedef std::string  QString;

//typedef std::basic_string<char, std::char_traits<char>, Bert::Allocator<char> >  QString;

class    QObject;
QObject  CreateStringObject(const QString&  value);
QObject  CreateStringObject(long value);

#endif
