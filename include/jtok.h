/* jtok.c */
struct JToken {
    int so;
    int eo;
    char *match;
};
void jtok_init(struct JToken *tok);
int jtok(struct JToken *tok, char *string, char delim);
int main(int argc, char *argv[]);
