#pragma once

#include "../../xrCore/XML_Parser/xrXMLParser.h"

class CUIXml : public CXml
{
    int m_dbg_id;

public:
    CUIXml();
    virtual ~CUIXml();

    virtual shared_str correct_file_name(LPCSTR path, LPCSTR fn);

    // UI anchors, see xrUIXmlParser.cpp
    bool IsKeepAspect(LPCSTR path, int index); // the node is laid out in square units
    virtual bool TransformLayoutAttrib(XML_NODE* node, LPCSTR attrib, LPCSTR raw_value, float default_value, float& result);
};