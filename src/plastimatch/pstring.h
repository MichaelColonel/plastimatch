/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _pstring_h_
#define _pstring_h_

#include "plm_config.h"
#include "bstrwrap.h"

class Pstring : public CBString
{
  public:
    Pstring () {}
    Pstring (CBString& s) : CBString (s) {}
    using CBString::operator =;
    const Pstring& operator = (const std::string& s) {
	(*this) = s.c_str();
	return *this;
    }
    
  public:
    bool empty (void) {
	return this->length() == 0;
    }
    bool not_empty (void) {
	return this->length() != 0;
    }
    /* Asymmetric comparison.  Return true if first section of Pstring 
       match prefix string.  */
    bool has_prefix (const char* prefix) {
	int pos;
	for (pos = 0; pos < this->slen; ++pos) {
	    if (prefix[pos] == 0) {
		return true;
	    }
	    if (this->data[pos] != prefix[pos]) {
		return false;
	    }
	}
	return (prefix[pos] == 0);
    }
};

#endif
