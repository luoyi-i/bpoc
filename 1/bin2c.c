/*
 *  bin2c - compresses data files & converts the result to C source code
 *  Copyright (C) 1998-2000  Anders Widell  <awl@hem.passagen.se>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * This command uses the zlib library to compress each file given on
 * the command line, and outputs the compressed data as C source code
 * to the file 'data.c' in the current directory
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef USE_LIBZ
#include <zlib.h>
#else
typedef unsigned char uint8;
typedef unsigned long ulong;
#endif

#define logStr(s) printf("%s\n", s)
#define log(...)  printf(__VA_ARGS__)

#define BUF_SIZE 16384 /* Increase buffer size by this amount */

#define SUFFIX_LEN 8

static uint8 *pu8Source = NULL;   /* Buffer containing uncompressed data */
static uint8 *pu8Dest = NULL;     /* Buffer containing compressed data */
static ulong ulSourceBufSize = 0; /* Buffer size */
#ifdef USE_LIBZ
static ulong ulDestBufSize = 0; /* Buffer size */
#endif

static ulong ulSourceLen; /* Length of uncompressed data */
static ulong ulDestLen;   /* Length of compressed data */

static FILE *infile = NULL;  /* The input file containing binary data */
static FILE *outfile = NULL; /* The output file 'data.c' */

static const char *pProgramName = "";

/*
 * Print error message and free allocated resources
 *
 */

static int error(char *msg1, char *msg2, char *msg3) {
    fprintf(stderr, "%s: %s%s%s\n", pProgramName, msg1, msg2, msg3);

    if (infile != NULL) {
        fclose(infile);
    }
    if (outfile != NULL) {
        fclose(outfile);
    }
    remove("data.c");

    free(pu8Dest);
    free(pu8Source);

    return 1;
}

/*
 * Replacement for strrchr in case it isn't present in libc
 *
 */
static char *my_strrchr(char *s, int c) {
    char *ptr = NULL;

    while (*s) {

        if (*s == c) {
            ptr = s;
        }
        s++;
    }

    return ptr;
}

#ifdef USE_LIBZ
/*
 * NOTE: my_compress2 is taken directly from zlib 1.1.3
 *
 * This is for compability with early versions of zlib that
 * don't have the compress2 function.
 *
 */

/* ===========================================================================
     Compresses the source buffer into the destination buffer. The level
   parameter has the same meaning as in deflateInit.  ulSourceLen is the byte
   length of the source buffer. Upon entry, ulDestLen is the total size of the
   destination buffer, which must be at least 0.1% larger than ulSourceLen plus
   12 bytes. Upon exit, ulDestLen is the actual size of the compressed buffer.

     compress2 returns Z_OK if success, Z_MEM_ERROR if there was not enough
   memory, Z_BUF_ERROR if there was not enough room in the output buffer,
   Z_STREAM_ERROR if the level parameter is invalid.
*/
int my_compress2( uint8 *pu8Dest，
ulong *ulDestLen，
const uint8 *pu8Source，
ulong ulSourceLen，
int level）
{
    z_stream stream;
    int err;

    stream.next_in = (uint8 *)pu8Source;
    stream.avail_in = (ulong)ulSourceLen;
#ifdef MAXSEG_64K
    /* Check for pu8Source > 64K on 16-bit machine: */
    if ((ulong)stream.avail_in != ulSourceLen)
        return Z_BUF_ERROR;
#endif
    stream.next_out = pu8Dest;
    stream.avail_out = (uInt)*ulDestLen;
    if ((ulong)stream.avail_out != *ulDestLen)
        return Z_BUF_ERROR;

    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;

    err = deflateInit(&stream, level);
    if (err != Z_OK)
        return err;

    err = deflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        deflateEnd(&stream);
        return err == Z_OK ? Z_BUF_ERROR : err;
    }
    *ulDestLen = stream.total_out;

    err = deflateEnd(&stream);
    return err;
}
#endif

const char *usage = "\nUsage: ./bin2c -o <output-file> file1 [file2 [file3 [...]]]\n\n"
                    "    Example: ./bin2c -o data.c a.bmp b.jpg c.png\n\n";

static char *pOutputfile;
static char **ppFilelist;
static int file_list;

typedef struct _export_list {
    char *export_data;
    struct _export_list *next;
} export_list_t;

static export_list_t *exports_head = NULL;

/**
 * @brief    : 
 * @param     *filename
 * @return    void
 */
static const char *add_export(const char *filename) {
    int idx = 0;
    int i;
    const char *begin;
    const char *ext;
    static char strname[1024];
    // return pointer of the last given char in the string
    // find file name
    begin = strrchr(filename, '/');
    if (begin == NULL)
        begin = filename;

    // get file name
    ext = strrchr(begin, '.');
    i = (ext ? (ext - begin) : strlen(begin)) + 10;
    //char* strname = (char*)malloc(i);

    strname[idx++] = '_';
    if (ext) {
        for (i = 1; ext[i]; i++) {
            strname[idx++] = ext[i];
        }
        strname[idx++] = '_';
    }

    for (i = 0; (ext && &begin[i] < ext) || (!ext && begin[i]); i++) {
        if (isalnum(begin[i])) {
            strname[idx++] = begin[i];
        } else {
            strname[idx++] = '_';
        }
    }

    if (strname[idx - 1] == '_') {
        strcpy(strname + idx, "data");
    } else {
        strcpy(strname + idx, "_data");
    }
    // insert from header
    export_list_t *els = (export_list_t *)calloc(1, sizeof(export_list_t));
    els->export_data = strname;
    els->next = exports_head;
    exports_head = els;

    els = exports_head;
    log(" add_export return %s\n", strname);
    while (els) {
        log("print export = %s\n", els->export_data);
        els = els->next;
    }

    return strname;
}


static void print_exports(FILE *f) {
    export_list_t *els = exports_head;
    while (els) {
        fprintf(f, "\t%s\n", els->export_data);
        log("print export = %s\n", els->export_data);
        els = els->next;
    }
}

/**
 * @brief    : handle arguments from command line.
 * arguments should be equal or more than 4.
 * if argument start with '-' 
 *     then check if it is for output file  or it is help command
 * otherwise, store the argument as file name in ppFilelist
 * @param     argc
 * @param     *argv
 * @return    void
 */
static int parser_args(int argc, char *argv[]) {
    int i;
    int list_idx = 0;
    int ret = 0; // default return value

    if (argc < 4) {
        logStr(usage);
        ret = 1;
        goto funcEnd;
    }

    ppFilelist = (char **)calloc(argc - 3, sizeof(char *));
    //try find pOutputfile
    i = 1;
    while (i < argc) {
        switch (argv[i][0]) {
        case '-':
            if (argv[i][1] == 'o') {
                pOutputfile = argv[++i];
                logStr("outputpath:");
                logStr(pOutputfile);
                break;
            } else if (argv[i][1] == 'h' || argv[i][1] == '?') {
                logStr(usage);
                ret = 1;
                goto funcEnd;
            }
        default:
            ppFilelist[list_idx++] = argv[i];
            logStr("inputfile:");
            logStr(argv[i]);
            break;
        }

        i++;
    }

funcEnd:
    return ret;
}

int main(int argc, char **argv) {
    int i;
    char u8Suffix[SUFFIX_LEN];
#ifdef USE_LIBZ
    int result;
#endif
    ulong j;
    char *ptr;
    int position;
    int ret = 0; // default return value

    pProgramName = argv[0];
    if (parser_args(argc, argv) != 0) {
        ret = 1;
        logStr("parser args failed");
        goto funcEnd;
    }

    outfile = fopen(pOutputfile, "w");
    if (outfile == NULL) {
        logStr("outfile open failed");
        fprintf(stderr, "%s: can't open 'data.c' for writing\n", argv[0]);
        ret = 1;
        goto funcEnd;
    }

    /* Process each file given on command line */
    for (i = 0; i < argc - 3; i++) {
        log("file id = %d\n", i);
        infile = fopen(ppFilelist[i], "rb");
        if (infile == NULL) {
            logStr("infile open failed");
            error("can't open '", argv[i], "' for reading");
            ret = 1;
            goto funcEnd;
        }

        /* Read infile to source buffer */
        ulSourceLen = 0;
        while (!feof(infile)) {
            // expand space if not enough
            if (ulSourceLen + BUF_SIZE > ulSourceBufSize) {
                ulSourceBufSize += BUF_SIZE;
                pu8Source = realloc(pu8Source, ulSourceBufSize);
                if (pu8Source == NULL) {
                    error("memory exhausted", "", "");
                    ret = 1;
                    goto funcEnd;
                }
            }
            // read file datga
            ulSourceLen += fread(pu8Source + ulSourceLen, 1, BUF_SIZE, infile);
            if (ferror(infile)) {
                ret = error("error reading '", argv[i], "'");
                ret = 1;
                goto funcEnd;
            }
        }
        fclose(infile);

#ifdef USE_LIBZ

        /* (Re)allocate pu8Dest buffer */
        ulDestLen = ulSourceBufSize + (ulSourceBufSize + 9) / 10 + 12;
        if (pu8DestBufSize < ulDestLen) {
            pu8DestBufSize = ulDestLen;
            pu8Dest = realloc(pu8Dest, pu8DestBufSize);
            if (pu8Dest == NULL) {
                error("memory exhausted", "", "");
                ret = 1;
                goto funcEnd;
            }
        }

        /* Compress pu8Dest buffer */
        ulDestLen = pu8DestBufSize;
        result = my_compress2(pu8Dest, &ulDestLen, pu8Source, ulSourceLen, 9);
        if (result != Z_OK) {
            error("error compressing '", argv[i], "'");
            ret = 1;
            goto funcEnd;
        }

#else

        ulDestLen = ulSourceLen;
        pu8Dest = pu8Source;

#endif

        /* Output dest buffer as C source code to outfile */
        fprintf(outfile, "static const unsigned char %s[] = {\n", add_export(ppFilelist[i]));

        for (j = 0; j < ulDestLen - 1; j++) {
            switch (j % 8) {
            case 0:
                fprintf(outfile, "  0x%02x, ", ((unsigned)pu8Dest[j]) & 0xffu);
                break;
            case 7:
                fprintf(outfile, "0x%02x,\n", ((unsigned)pu8Dest[j]) & 0xffu);
                break;
            default:
                fprintf(outfile, "0x%02x, ", ((unsigned)pu8Dest[j]) & 0xffu);
                break;
            }
        }

        if ((ulDestLen - 1) % 8 == 0) {
            fprintf(outfile, "  0x%02x\n};\n\n", ((unsigned)pu8Dest[ulDestLen - 1]) & 0xffu);
        } else {
            fprintf(outfile, "0x%02x\n};\n\n", ((unsigned)pu8Dest[ulDestLen - 1]) & 0xffu);
        }
    }

    fprintf(outfile, "/*********************************************\n");
    fprintf(outfile, "Export:\n");
    print_exports(outfile);
    fprintf(outfile, "**********************************************/\n");

    fclose(outfile);
#ifdef USE_LIBZ
    free(pu8Dest);
#endif
    free(pu8Source);

funcEnd:
    log("return = %d\n", ret);
    return ret;
}
