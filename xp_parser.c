#include <stdio.h>
#include <string.h>
#include <stdbool.h> // bool, true, fals



#define strstartswith(haystack, needle) \
    (!strncmp(haystack, needle, sizeof(needle) - 1))


/************* Constants and Global variables ***********/

#define XP_MAX_NAME_LEN   256
#define XP_MAX_FILE_LEN   65536
#define XP_MAX_STACK_LEN  256

char  xp_file     [XP_MAX_FILE_LEN + 1];
char *xp_position [XP_MAX_STACK_LEN];
int   xp_stack    = 0;


/****************** Internal routines ********************/
char *xp_open_element(int index);
int xp_set_xml_buffer_from_file(const char *filename);
static char *xp_find_start_tag_end(char *ptr);
const char *xp_get_value(const char *name);
void xp_close_element(void);


void xp_close_element(void)
{
    if (xp_stack) {
        xp_stack--;
    }
}
/*
 * name="Basic UAS responder", for this xp_stack, the name point to
 * "name"
 * @return: Basic UAS responder
 */ 
const char *xp_get_value(const char *name)
{
    int index = 0;
    static char buffer[XP_MAX_FILE_LEN + 1];
    char *ptr, *end, *check;

    end = xp_find_start_tag_end(xp_position[xp_stack] + 1);
    if (!end)
        return NULL;

    ptr = xp_position[xp_stack];

    while (*ptr) {
        ptr = strstr(ptr, name);

        if (!ptr)
            return NULL;
        if (ptr > end)
            return NULL;
        /* FIXME: potential BUG in parser: we must retrieve full word,
         *          * so the use of strstr as it is is not enough.
         *                   * we should check that the retrieved word is not a piece of
         *                            * another one. */
        check = ptr - 1;
        if (check >= xp_position[xp_stack]) {
            if ((*check != '\r') &&
                    (*check != '\n') &&
                    (*check != '\t') &&
                    (*check != ' ' )) {
                ptr += strlen(name);
                continue;
            }
        } else
            return(NULL);

        ptr += strlen(name);
        while ((*ptr == '\r') ||
                (*ptr == '\n') ||
                (*ptr == '\t') ||
                (*ptr == ' ' )    ) {
            ptr++;
        }
        if (*ptr != '=')
            continue;
        ptr++;
        while ((*ptr == '\r') ||
                (*ptr == '\n') ||
                (*ptr == '\t') ||
                (*ptr ==  ' ')    ) {
            ptr++;
        }
        ptr++;                      /* skip the " */
        if (*ptr) {
            while (*ptr) {
                if (*ptr == '\\') {
                    ptr++;
                    switch(*ptr) {
                        case '\\':
                            buffer[index++] = '\\';
                            break;
                        case '"':
                            buffer[index++] = '"';
                            break;
                        case 'n':
                            buffer[index++] = '\n';
                            break;
                        case 't':
                            buffer[index++] = '\t';
                            break;
                        case 'r':
                            buffer[index++] = '\r';
                            break;
                        default:
                            buffer[index++] = '\\';
                            buffer[index++] = *ptr;
                            break;
                    }
                    ptr++;
                } else if (*ptr == '"') {
                    break;
                } else {
                    buffer[index++] = *ptr++;
                }
                if (index > XP_MAX_FILE_LEN)
                    return NULL;
            }
            buffer[index] = 0;
            return buffer;
        }
    }
    return NULL;
}

/* This finds the end of something like <send foo="bar">, and does not recurse
 *  * into other elements. */
static char *xp_find_start_tag_end(char *ptr)
{
    while (*ptr) {
        if (*ptr == '<') {
            if (strstartswith(ptr, "<!--")) {
                char *comment_end = strstr(ptr, "-->");
                if (!comment_end)
                    return NULL;
                ptr = comment_end + 3;
            } else {
                return NULL;
            }
        } else if ((*ptr == '/') && (*(ptr+1) == '>')) {
            return ptr;
        } else if (*ptr == '"') {
            ptr++;
            while (*ptr) {
                if (*ptr == '\\') {
                    ptr += 2;
                } else if (*ptr == '"') {
                    ptr++;
                    break;
                } else {
                    ptr++;
                }
            }
        } else if (*ptr == '>') {
            return ptr;
        } else {
            ptr++;
        }
    }
    return ptr;
}

int xp_set_xml_buffer_from_file(const char *filename)
{
    FILE *f = fopen(filename, "rb");
    char *pos;
    int index = 0;
    int c;

    if (!f) {
        return 0;
    }

    while ((c = fgetc(f)) != EOF) {
        if (c == '\r')
            continue;
        xp_file[index++] = c;
        if (index >= XP_MAX_FILE_LEN) {
            xp_file[index++] = 0;
            xp_stack = 0;
            xp_position[xp_stack] = xp_file;
            fclose(f);
            return 0;
        }
    }
    xp_file[index++] = 0;
    fclose(f);

    xp_stack = 0;
    xp_position[xp_stack] = xp_file;

    if (!strstartswith(xp_position[xp_stack], "<?xml"))
        return 0;
    if (!(pos = strstr(xp_position[xp_stack], "?>")))
        return 0;
    xp_position[xp_stack] = pos + 2;
    return 1;
}

char *xp_open_element(int index)
{
    char *ptr = xp_position[xp_stack];
    int level = 0;
    static char name[XP_MAX_NAME_LEN];

    while (*ptr) {
        if (*ptr == '<') {
            if ((*(ptr+1) == '!') &&
                    (*(ptr+2) == '[') &&
                    (strstr(ptr, "<![CDATA[") == ptr)) {
                char *cdata_end = strstr(ptr, "]]>");
                if (!cdata_end)
                    return NULL;
                ptr = cdata_end + 2;

            } else if ((*(ptr+1) == '!') &&
                    (*(ptr+2) == '-') &&
                    (strstr(ptr, "<!--") == ptr)) {
                char *comment_end = strstr(ptr, "-->");
                if (!comment_end)
                    return NULL;
                ptr = comment_end + 2;

            } else if (strstartswith(ptr, "<!DOCTYPE")) {
                char *doctype_end = strstr(ptr, ">");
                if (!doctype_end)
                    return NULL;
                ptr = doctype_end;
            } else if (strstartswith(ptr, "<?xml-model")) {
                char *xmlmodel_end = strstr(ptr, ">");
                if (!xmlmodel_end)
                    return NULL;
                ptr = xmlmodel_end;
            } else if (*(ptr+1) == '/') {
                level--;
                if (level < 0)
                    return NULL;
            } else {
                if (level == 0) {
                    if (index) {
                        index--;
                    } else {
                        char *end = xp_find_start_tag_end(ptr + 1);
                        char *p;
                        if (!end) {
                            return NULL;
                        }
                        p = strchr(ptr, ' ');
                        if (p && (p < end))  {
                            end = p;
                        }
                        p = strchr(ptr, '\t');
                        if (p && (p < end))  {
                            end = p;
                        }
                        p = strchr(ptr, '\r');
                        if (p && (p < end))  {
                            end = p;
                        }
                        p = strchr(ptr, '\n');
                        if (p && (p < end))  {
                            end = p;
                        }
                        p = strchr(ptr, '/');
                        if (p && (p < end))  {
                            end = p;
                        }
                        memcpy(name, ptr + 1, end-ptr-1);
                        name[end-ptr-1] = 0;

                        xp_position[++xp_stack] = end;
                        return name;
                    }
                }

                /* We want to skip over this particular element .*/
                ptr = xp_find_start_tag_end(ptr + 1);
                if (!ptr)
                    return NULL;
                ptr--;
                level++;
            }
        } else if ((*ptr == '/') && (*(ptr+1) == '>')) {
            level--;
            if (level < 0)
                return NULL;
        }
        ptr++;
    }
    return NULL;
}


int main()
{
    char * filename = "uas.xml";
    int i = 0;
    char * elem;
    char * name;
    const char* cptr;
    int scenario_file_cursor = 0;
    bool dump = true;

    if(!xp_set_xml_buffer_from_file(filename)){
        printf("Unable to load or parse '%s' xml scenario file", filename);
        return 0;
    }

    elem = xp_open_element(0);
    if (!elem) {
        printf("No element in xml scenario file");
        return 0;
    }

    if(strcmp("scenario", elem)) {
        printf("No 'scenario' section in xml scenario file");

    }

    if ((cptr = xp_get_value("name"))) {
        name = strdup(cptr);
    } else {
        name = strdup("");
    }

    while (( elem = xp_open_element(scenario_file_cursor))){
        char * ptr;
        scenario_file_cursor++;

        if(dump){
            dump = false;
            do
            {
                printf("%d -- %s", i, xp_position[i]);
                i++;
            }while(i <= xp_stack);

        }
        if(!strcmp(elem, "send")) {
            printf("send\n");

        } else if (!strcmp(elem, "recv")) {
            printf("recv\n");
        }else {
            printf("others\n");
        }
        xp_close_element();
    }


    return 1;
}
