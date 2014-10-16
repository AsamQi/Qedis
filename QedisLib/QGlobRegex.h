#ifndef BERT_QGLOBREGEX_H
#define BERT_QGLOBREGEX_H

#include "QString.h"

class QGlobRegex
{
public:
    explicit QGlobRegex(const char* pattern = 0, std::size_t plen = 0,
                        const char* text = 0, std::size_t tlen = 0);

    void SetPattern(const char* pattern, std::size_t plen);
    void SetText(const char* text, std::size_t tlen);
    bool TryMatch();

private:
    bool _ProcessStar();
    bool _ProcessQuestion();
    bool _ProcessBracket();
    bool _IsMatch() const;

    const char*     m_pattern;
    std::size_t     m_pLen;
    std::size_t     m_pOff;

    const char*     m_text;
    std::size_t     m_tLen;
    std::size_t     m_tOff;
};

inline bool glob_match(const char* pattern, std::size_t plen,
                       const char* text, std::size_t tlen)
{
    QGlobRegex   rgx;
    rgx.SetPattern(pattern, plen);
    rgx.SetText(text, tlen);

    return rgx.TryMatch();
}

inline bool glob_match(const char* pattern,
                       const char* text)
{
    return glob_match(pattern, strlen(pattern), text, strlen(text));
}

inline bool glob_match(const QString& pattern,
                       const QString& text)
{
    return glob_match(pattern.c_str(), pattern.size(),
                      text.c_str(), text.size());
}

// TODO search
inline bool glob_search(const char* pattern,
                       const char* str)
{
    QString sPattern("*");
    sPattern += pattern;
    sPattern += "*";

    QGlobRegex   rgx;
    rgx.SetPattern(sPattern.c_str(), sPattern.size());
    rgx.SetText(str, strlen(str));

    return rgx.TryMatch();
}

#endif

