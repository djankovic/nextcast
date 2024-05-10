#ifndef PARSECONFIG_H
#define PARSECONFIG_H

// taken from camE http://linuxbrit.co.uk/camE/

int cfg_parse_file(const char *filename);
char **cfg_list_sections(void);
char **cfg_list_entries(const char *name);
char *cfg_get_str(const char *sec, const char *ent, const char *def_val);
int cfg_get_int(const char *sec, const char *ent, const int def_val);
float cfg_get_float(const char *sec, const char *ent, const float def_val);
void cfg_free(void);

#endif /* PARSECONFIG_H */
