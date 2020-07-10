#include <stdio.h>
#include <stdlib.h>

/*
 * jtok Copyright (C) 2020 Jess Mahan <code@alienrobotarmy.com>
 *
 * jtok is a *better* string tokenizing routine. use this
 * in place of strtok(), strsep(), etc.
 *
 * int jtok(struct tokens *tok, char *string, char delim)
 *
 *
 * Usage:
 * 
 * struct JToken tok;
 * tok.so = 0; tok.eo = 0; tok.match = NULL;
 * jtok(&tok, "this,is,a,test", ',');
 * free(tok.match);
 *
 * 
 * Return Value:
 *
 * jtok returns an integer.
 * 0 = No token found;
 * 1 = Token found;
 * Even if no token was found jtok will still copy the contents of
 * char *string into tok.match, which must be freed after the final
 * call to jtok.
 *
 *
 * Notes:
 *
 * You mush initialize the JToken struct before using:
 *   tok.so = 0; tok.eo = 0; tok.match = NULL;
 * Failure to do so will result in unexpected behaviour.
 *
 *
 * Example:
 *
 * int main(int argc, char *argv[]) {
 *   int ret = 0;
 *   struct JToken tok;
 *   tok.so = 0; tok.eo = 0; tok.match = NULL;
 *   while ((ret = jtok(&tok, argv[1], ',')) > 0) {
 *     fprintf(stdout, "%s\n", tok.match);
 *   }
 *   free(tok.match);
 *   return 0;
 * }
 */

struct JToken
{
    int so;
    int eo;
    char *match;
};

void jtok_init(struct JToken *tok)
{
    if (tok->match)
        free(tok->match);
    tok->so = 0;
    tok->eo = 0;
    tok->match = NULL;
}
int jtok(struct JToken *tok, char *string, char delim)
{
    int i = 0, x = 0, len = 0;
    char *p = string;

    for (len = 0; *string++; len++)
        ;
    ;
    if (tok->so > 0 && tok->eo >= len)
        return 0;
    string = p;

    if (tok->eo > 0)
        tok->eo++;
    tok->so = tok->eo;

    if (tok->so == 0)
        while (*string++ == delim)
            tok->so = tok->eo++;
    string = p;

    string += tok->so = tok->eo;

    for (i = tok->so; *string; i++)
    {
        if (*string == delim)
        {
            tok->eo = i;
            if (tok->match)
                free(tok->match);
            tok->match = calloc(sizeof(char), (tok->eo - tok->so) + 1);
            for (x = tok->so; x < tok->eo; x++)
                tok->match[x - tok->so] = p[x];
            tok->match[x - tok->so] = '\0';
            string = p;
            return 1;
        }
        string++;
    }
    if (tok->eo < len)
    {
        tok->eo = len;
        if (tok->match)
            free(tok->match);
        tok->match = calloc(sizeof(char), (tok->eo - tok->so) + 1);
        for (x = tok->so; x < tok->eo; x++)
            tok->match[x - tok->so] = p[x];
        tok->match[x - tok->so] = '\0';
        string = p;
        return 1;
    }
    string = p;
    return 0;
}

/*
int main(int argc, char *argv[])
{
    struct JToken *tok = malloc(sizeof(struct JToken));
    int ret = 0;
    char delim = ':';

    tok->so = 0;
    tok->eo = 0;
    tok->match = NULL;
    while ((ret = jtok(tok, argv[1], delim)) > 0) 
	fprintf(stdout, "[%s]\n", tok->match);

    free(tok->match);
    free(tok);
    return 0;
}
*/
