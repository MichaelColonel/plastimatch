/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmsys_config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "string_util.h"

int
plm_strcmp (const char* s1, const char* s2)
{
    return strncmp (s1, s2, strlen(s2));
}

void
string_util_rtrim_whitespace (char *s)
{
    int len = strlen (s);
    while (len > 0 && isspace(s[len-1])) {
	s[len-1] = 0;
	len--;
    }
}

int
parse_int13 (int *arr, const char *string)
{
    int rc;
    rc = sscanf (string, "%d %d %d", &arr[0], &arr[1], &arr[2]);
    if (rc == 3) {
	return 0;
    } else if (rc == 1) {
	arr[1] = arr[2] = arr[0];
	return 0;
    } else {
	/* Failure */
	return 1;
    }
}

int
parse_dicom_float2 (float *arr, const char *string)
{
    int rc;
    rc = sscanf (string, "%f\\%f", &arr[0], &arr[1]);
    if (rc == 2) {
	return 0;
    } else {
	/* Failure */
	return 1;
    }
}

int
parse_dicom_float3 (float *arr, const char *string)
{
    int rc;
    rc = sscanf (string, "%f\\%f\\%f", &arr[0], &arr[1], &arr[2]);
    if (rc == 3) {
	return 0;
    } else {
	/* Failure */
	return 1;
    }
}

int
parse_dicom_float6 (float *arr, const char *string)
{
    int rc;
    rc = sscanf (string, "%f\\%f\\%f\\%f\\%f\\%f", 
	&arr[0], &arr[1], &arr[2], &arr[3], &arr[4], &arr[5]);
    if (rc == 6) {
	return 0;
    } else {
	/* Failure */
	return 1;
    }
}

/* Parse a string of the form "3 22 -1; 3 4 66; 3 1 0" */
std::vector<int>
parse_int3_string (const char* s)
{
    std::vector<int> int_list;
    const char* p = s;
    int rc = 0;
    int n;

    do {
	int v[3];

	n = 0;
	rc = sscanf (p, "%d %d %d;%n", &v[0], &v[1], &v[2], &n);
	p += n;
	if (rc >= 3) {
	    int_list.push_back (v[0]);
	    int_list.push_back (v[1]);
	    int_list.push_back (v[2]);
	}
    } while (rc >= 3 && n > 0);
    return int_list;
}

/* Parse a string of the form "3 22 -1; 3 4 66; 3 1 0" */
std::vector<float>
parse_float3_string (const char* s)
{
    std::vector<float> float_list;
    const char* p = s;
    int rc = 0;
    int n;

    do {
	float v[3];

	n = 0;
	rc = sscanf (p, "%f %f %f;%n", &v[0], &v[1], &v[2], &n);
	p += n;
	if (rc >= 3) {
	    float_list.push_back (v[0]);
	    float_list.push_back (v[1]);
	    float_list.push_back (v[2]);
	}
    } while (rc >= 3 && n > 0);
    return float_list;
}


/* String trimming by GMan.
   http://stackoverflow.com/questions/1798112/removing-leading-and-trailing-spaces-from-a-string/1798170#1798170
   Distributed under Attribution-ShareAlike 3.0 Unported license (CC BY-SA 3.0) 
   http://creativecommons.org/licenses/by-sa/3.0/
*/
const std::string
trim (
    const std::string& str,
    const std::string& whitespace
)
{
    const size_t begin_str = str.find_first_not_of (whitespace);
    if (begin_str == std::string::npos)
    {
        // no content
        return "";
    }

    const size_t end_str = str.find_last_not_of(whitespace);
    const size_t range = end_str - begin_str + 1;

    return str.substr (begin_str, range);
}