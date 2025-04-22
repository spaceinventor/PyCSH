#include <csh/url_utils.h>
#include <string.h>

bool is_http_url(const char *input) {
    if(NULL == input) {
        return false;
    }
    size_t len = strlen(input);
    if(len < 10) { /* len(http://a.b) = 10*/
        return false;
    }
    char *found = strstr(input, "http://");
    if (NULL == found) {
        found = strstr(input, "https://");
        if(NULL == found) {
            return false;
        } else {
            if(len < 11) { /* len(https://a.b) = 11 */
                return false;
            }
        }
    }
    return true;
}