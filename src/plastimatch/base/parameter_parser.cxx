/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fstream>
#include <iostream>
#include <sstream>

#include "file_util.h"
#include "logfile.h"
#include "parameter_parser.h"
#include "print_and_exit.h"
#include "string_util.h"

Parameter_parser::Parameter_parser () {
    key_regularization = true;
}

int
Parameter_parser::parse_config_string (
    const char* config_string
)
{
    std::stringstream ss (config_string);
    std::string buf;
    std::string buf_ori;    /* An extra copy for diagnostics */
    std::string section = "GLOBAL";

    while (getline (ss, buf)) {
        buf_ori = buf;
        buf = trim (buf);
        buf_ori = trim (buf_ori, "\r\n");

        if (buf == "") continue;
        if (buf[0] == '#') continue;

        /* Process "[SECTION]" */
        if (buf[0] == '[') {
            if (buf[buf.length()-1] != ']') {
                lprintf ("Parse error: %s\n", buf_ori.c_str());
                return -1;
            }

            /* Strip off brackets and make upper case */
            buf = buf.substr (1, buf.length()-2);
            section = make_uppercase (buf);

            /* Inform subclass that a new section is beginning */
            int rc = this->process_section (section);
            if (rc != 0) {
                lprintf ("Parse error: %s\n", buf_ori.c_str());
                return rc;
            }
            continue;
        }

        /* Process "key=value" */
        std::string key;
        std::string val;
        size_t key_loc = buf.find ("=");
        if (key_loc == std::string::npos) {
            key = buf;
            val = "";
        } else {
            key = buf.substr (0, key_loc);
            val = buf.substr (key_loc+1);
        }
        key = trim (key);
        if (this->key_regularization) {
            key = regularize_string (key);
        }
        val = trim (val);

        if (key != "") {
            int rc = this->process_key_value (section, key, val);
            if (rc != 0) {
                lprintf ("Parse error: %s\n", buf_ori.c_str());
                return rc;
            }
        }
    }
    return 0;
}

void 
Parameter_parser::enable_key_regularization (
    bool enable
)
{
    this->key_regularization = enable;
}

int
Parameter_parser::parse_config_string (
    const std::string& config_string
)
{
    return this->parse_config_string (config_string.c_str());
}

int
Parameter_parser::parse_config_file (
    const char* config_fn
)
{
    /* Confirm file can be read */
    if (!file_exists (config_fn)) {
        print_and_exit ("Error reading config file: %s\n", config_fn);
    }

    /* Read file into string */
    std::ifstream t (config_fn);
    std::stringstream buffer;
    buffer << t.rdbuf();

    return this->parse_config_string (buffer.str());
}
