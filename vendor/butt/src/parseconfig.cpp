/*
 * config file parser
 *
 */

// taken from camE http://linuxbrit.co.uk/camE/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <FL/fl_utf8.h> // for fl_fopen(...)

#include "parseconfig.h"

struct CFG_ENTRIES {
    unsigned int ent_count;
    char **ent_names;
    char **ent_values;
};

struct CFG_SECTIONS {
    unsigned int sec_count;
    char **sec_names;
    struct CFG_ENTRIES **sec_entries;
};

/* ------------------------------------------------------------------------ */

static struct CFG_SECTIONS *c;

/* ------------------------------------------------------------------------ */

enum {
    ALLOC_SIZE = 16,
};

void cfg_free(void)
{
    unsigned int i,j;
    struct CFG_ENTRIES *e;
    
    if (c == NULL) {
        return;
    }
    
    for (i = 0; i < c->sec_count; i++) {
        e = c->sec_entries[i];
        printf("ent_count: %d\n", e->ent_count);
        for (j = 0; j < e->ent_count; j++) {
            printf("i,j: %d,%d ", i,j);
            printf("name,value: %s,%s\n", e->ent_names[j], e->ent_values[j]);
            free(e->ent_names[j]);
            free(e->ent_values[j]);
        }
        
       free(e->ent_names);
       free(e->ent_values);
       free(c->sec_entries[i]);
       free(c->sec_names[i]);
    }
    
    free(c->sec_names);
    free(c->sec_entries);
    
    c = NULL;
}

static struct CFG_SECTIONS *cfg_init_sections(void)
{
    struct CFG_SECTIONS *c;
    c = (CFG_SECTIONS *)malloc(sizeof(struct CFG_SECTIONS));
    memset(c, 0, sizeof(struct CFG_SECTIONS));
    c->sec_names = (char **)malloc(ALLOC_SIZE * sizeof(char *));
    c->sec_names[0] = NULL;
    c->sec_entries = (CFG_ENTRIES **)malloc(ALLOC_SIZE * sizeof(struct CFG_ENTRIES *));
    c->sec_entries[0] = NULL;
    return c;
}

static struct CFG_ENTRIES *cfg_init_entries(void)
{
    struct CFG_ENTRIES *e;
    e = (CFG_ENTRIES *)malloc(sizeof(struct CFG_ENTRIES));
    memset(e, 0, sizeof(struct CFG_ENTRIES));
    e->ent_names = (char **)malloc(ALLOC_SIZE * sizeof(char *));
    e->ent_names[0] = NULL;
    e->ent_values = (char **)malloc(ALLOC_SIZE * sizeof(char *));
    e->ent_values[0] = NULL;
    return e;
}

static struct CFG_ENTRIES *cfg_find_section(struct CFG_SECTIONS *c, const char *name)
{
    struct CFG_ENTRIES *e;
    unsigned int i;

    for (i = 0; i < c->sec_count; i++) {
        if (strcmp(c->sec_names[i], name) == 0) {
            return c->sec_entries[i];
        }
    }

    /* 404 not found => create a new one */
    if ((c->sec_count % ALLOC_SIZE) == (ALLOC_SIZE - 2)) {
        c->sec_names = (char **)realloc(c->sec_names, (c->sec_count + 2 + ALLOC_SIZE) * sizeof(char *));
        c->sec_entries = (CFG_ENTRIES **)realloc(c->sec_entries, (c->sec_count + 2 + ALLOC_SIZE) * sizeof(struct CFG_ENTRIES *));
    }
    e = cfg_init_entries();
    c->sec_names[c->sec_count] = strdup(name);
    c->sec_entries[c->sec_count] = e;
    c->sec_count++;
    c->sec_names[c->sec_count] = NULL;
    c->sec_entries[c->sec_count] = NULL;
    return e;
}



static void cfg_set_entry(struct CFG_ENTRIES *e, const char *name, const char *value)
{
    unsigned int i;

    for (i = 0; i < e->ent_count; i++) {
        if (strcmp(e->ent_names[i], name) == 0) {
            break;
        }
    }
    if (i == e->ent_count) {
        /* 404 not found => create a new one */
        if ((e->ent_count % ALLOC_SIZE) == (ALLOC_SIZE - 2)) {
            e->ent_names = (char **)realloc(e->ent_names, (e->ent_count + 2 + ALLOC_SIZE) * sizeof(char *));
            e->ent_values = (char **)realloc(e->ent_values, (e->ent_count + 2 + ALLOC_SIZE) * sizeof(char *));
        }
        e->ent_count++;
        e->ent_names[e->ent_count] = NULL;
        e->ent_values[e->ent_count] = NULL;
    }
    e->ent_names[i] = strdup(name);
    e->ent_values[i] = strdup(value);
}

/* ------------------------------------------------------------------------ */

int cfg_parse_file(const char *filename)
{
    struct CFG_ENTRIES *e = NULL;
    char line[1024], tag[64], value[1021];
    FILE *fp;
    int nr;

    if (c != NULL) {
        cfg_free();
    }
    
    c = cfg_init_sections();
    if ((fp = fl_fopen(filename, "rb")) == NULL) {
        return -1;
    }

    nr = 0;
    while (NULL != fgets(line, 1023, fp)) {
        nr++;
        if (line[0] == '\n' || line[0] == '#' || line[0] == '%' || line[0] == ';') {
            continue;
        }
        if (1 == sscanf(line, "[%1020[^]]]", value)) {
            /* section */
            e = cfg_find_section(c, value);
        }
        else if (2 == sscanf(line, " %63[^= ] = %1020[^\n]", tag, value)) {
            /* foo = bar */
            if (NULL == e) {
                fprintf(stderr, "%s:%d: error: no section\n", filename, nr);
            }
            else {
                cfg_set_entry(e, tag, value);
            }
        } /*else {
        // Huh ?
        fprintf(stderr,"%s:%d: syntax error\n",filename,nr);
        }*/
    }
    fclose(fp);
    return 0;
}



/* ------------------------------------------------------------------------ */

char **cfg_list_sections(void)
{
    return c->sec_names;
}

char **cfg_list_entries(const char *name)
{
    unsigned int i;

    for (i = 0; i < c->sec_count; i++) {
        if (0 == strcmp(c->sec_names[i], name)) {
            return c->sec_entries[i]->ent_names;
        }
    }
    return NULL;
}

char *cfg_get_str(const char *sec, const char *ent, const char *def_val)
{
    struct CFG_ENTRIES *e = NULL;
    char *v = NULL;
    unsigned int i;

    for (i = 0; i < c->sec_count; i++) {
        if (strcmp(c->sec_names[i], sec) == 0) {
            e = c->sec_entries[i];
        }
    }
    if (e == NULL) {
        if (def_val == NULL) {
            return NULL;
        }
        else {
            v = (char *)malloc(strlen(def_val) + 1);
            strcpy(v, def_val);
            return v;
        }
    }
    for (i = 0; i < e->ent_count; i++) {
        if (strcmp(e->ent_names[i], ent) == 0) {
            v = e->ent_values[i];
        }
    }
    if (v == NULL) {
        if (def_val == NULL) {
            return NULL;
        }
        else {
            v = (char *)malloc(strlen(def_val) + 1);
            strcpy(v, def_val);
            return v;
        }
    }
    return strdup(v);
}

int cfg_get_int(const char *sec, const char *ent, const int def_val)
{
    char *val;

    val = cfg_get_str(sec, ent, NULL);
    if (val == NULL) {
        return def_val;
    }
    return atoi(val);
}

float cfg_get_float(const char *sec, const char *ent, const float def_val)
{
    char *val;

    val = cfg_get_str(sec, ent, NULL);
    if (val == NULL) {
        return def_val;
    }
    return atof(val);
}
