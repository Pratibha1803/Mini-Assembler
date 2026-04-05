
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_LINES 4096
#define MAX_TOKENS 32
#define MAX_BYTES  64
#define MAX_SYMS   4096

typedef enum { SEC_NONE, SEC_DATA, SEC_BSS, SEC_TEXT } Section;
typedef enum { OP_NONE, OP_REG, OP_IMM, OP_MEM } OperandKind;

typedef struct {
    int base;      // 0..7 for eax..edi or -1
    int index;     // 0..7 for eax..edi or -1
    int scale;     // 1/2/4/8 or 0
    int32_t disp;  // displacement
} MemAddr;

typedef struct {
    OperandKind kind;
    int reg;        // 0..7 for eax..edi
    int32_t imm;    // immediate
    MemAddr mem;
    int width32;    // memory size: dword (1) — default
} Operand;

typedef struct {
    char label[64];
    Section sec;
    uint32_t off;
} Symbol;

typedef struct {
    char text[256];
    Section sec;
    uint32_t off;      // offset within section at start of line (second pass)
    uint8_t bytes[MAX_BYTES];
    int blen;
} Line;

static Line lines[MAX_LINES];
static int nlines = 0;

static Symbol syms[MAX_SYMS];
static int nsyms = 0;

static Section cursec = SEC_NONE;
static uint32_t off_data = 0, off_bss = 0, off_text = 0;

static const char *regnames[8] = {"eax","ecx","edx","ebx","esp","ebp","esi","edi"};

static int reg_index(const char *s){
    for(int i=0;i<8;i++) if(strcmp(s, regnames[i])==0) return i;
    return -1;
}

static void trim(char *s){
    char *p=s; while(isspace((unsigned char)*p)) p++;
    if(p!=s) memmove(s,p,strlen(p)+1);
    for(int i=(int)strlen(s)-1;i>=0;i--){
        if(isspace((unsigned char)s[i])) s[i]=0; else break;
    }
}

static int parse_int(const char *s, int32_t *out){
    char *end=NULL;
    long v = 0;
    if (s[0]=='0' && s[1]=='x') v = strtol(s, &end, 16);
    else v = strtol(s, &end, 10);
    if(end==s) return 0;
    *out = (int32_t)v; return 1;
}

static void add_symbol(const char *label){
    if(nsyms>=MAX_SYMS) return;
    strncpy(syms[nsyms].label,label,sizeof(syms[nsyms].label)-1);
    syms[nsyms].label[sizeof(syms[nsyms].label)-1] = 0;
    syms[nsyms].sec = cursec;
    syms[nsyms].off = (cursec==SEC_DATA?off_data:(cursec==SEC_BSS?off_bss:off_text));
    nsyms++;
}

static const Symbol* find_symbol(const char *label){
    for(int i=0;i<nsyms;i++) if(strcmp(syms[i].label,label)==0) return &syms[i];
    return NULL;
}

static int tokenize(char *line, char *tok[], int maxtok){
    int n=0; char *p=line;
    while(*p){
        while(isspace((unsigned char)*p)) p++;
        if(!*p) break;
        if(*p==',' || *p=='[' || *p==']' || *p=='+' || *p=='-' || *p=='*'){
            char t[2]={*p,0};
            tok[n++]=strdup(t); p++;
        } else {
            char buf[128]; int k=0;
            while(*p && !isspace((unsigned char)*p) && *p!=',' && *p!='[' && *p!=']' && *p!='+' && *p!='-' && *p!='*'){
                buf[k++]=*p++; if(k>=127) break;
            }
            buf[k]=0; tok[n++]=strdup(buf);
        }
        if(n>=maxtok) break;
    }
    return n;
}

static int parse_mem(char *tok[], int n, int *i, MemAddr *m, int *width32_out){
    int width32 = 1; // default dword

    if(*i<n && strcasecmp(tok[*i],"dword")==0){
        width32 = 1;
        (*i)++;
    }

    if(*i>=n || strcmp(tok[*i],"[")!=0) return 0;
    (*i)++;
    m->base=-1; m->index=-1; m->scale=0; m->disp=0;
    int sign = +1;

    while(*i<n && strcmp(tok[*i],"]")!=0){
        if(strcmp(tok[*i],"+")==0){ sign=+1; (*i)++; continue; }
        if(strcmp(tok[*i],"-")==0){ sign=-1; (*i)++; continue; }

        int r = reg_index(tok[*i]);
        if(r>=0){
            // register: could be base or index before * scale
            if(*i+1<n && strcmp(tok[*i+1],"*")==0){
                // index * scale
                m->index = r; (*i)+=2;
                int32_t sc=0; 
                if(*i<n && parse_int(tok[*i],&sc)){ 
                    m->scale=(int)sc; (*i)++; 
                } else m->scale=1;
            } else {
                if(m->base==-1) m->base=r; else m->index=r;
                (*i)++;
            }
        } else {
            // number or symbol used as displacement
            int32_t v=0;
            if(parse_int(tok[*i],&v)){
                m->disp += sign*v; sign=+1; (*i)++;
            } else {
                const Symbol *S = find_symbol(tok[*i]);
                if(S){
                    m->disp += sign * (int32_t)S->off;
                    sign=+1; (*i)++;
                } else {
                    return 0;
                }
            }
        }
    }

    if(*i<n && strcmp(tok[*i],"]")==0) (*i)++; else return 0;
    if(m->index!=-1 && m->scale==0) m->scale=1;
    if(!(m->scale==0 || m->scale==1 || m->scale==2 || m->scale==4 || m->scale==8)) return 0;

    if(width32_out) *width32_out = width32;
    return 1;
}

static int parse_operand(char *tok[], int n, int *i, Operand *op){
    memset(op,0,sizeof(*op));
    op->width32 = 1;

    int mem_probe_i = *i;
    if(mem_probe_i<n && (strcmp(tok[mem_probe_i],"[")==0 || strcasecmp(tok[mem_probe_i],"dword")==0)){
        op->kind = OP_MEM;
        if(parse_mem(tok,n,i,&op->mem,&op->width32)) return 1;
        return 0;
    }

    int r = (*i<n?reg_index(tok[*i]):-1);
    if(r>=0){ op->kind=OP_REG; op->reg=r; (*i)++; return 1; }

    int32_t v=0;
    if(*i<n && parse_int(tok[*i],&v)){ op->kind=OP_IMM; op->imm=v; (*i)++; return 1; }

    const Symbol *S = (*i<n?find_symbol(tok[*i]):NULL);
    if(S){ op->kind=OP_IMM; op->imm=(int32_t)S->off; (*i)++; return 1; }

    return 0;
}

// Encoding helpers
static void emit_u8(Line *L, uint8_t b){ if(L->blen<MAX_BYTES) L->bytes[L->blen++]=b; }
static void emit_u32(Line *L, uint32_t v){
    emit_u8(L,(uint8_t)(v&0xFF));
    emit_u8(L,(uint8_t)((v>>8)&0xFF));
    emit_u8(L,(uint8_t)((v>>16)&0xFF));
    emit_u8(L,(uint8_t)((v>>24)&0xFF));
}

// Choose mod and displacement size:
// - No base/index: disp32 only, mod=00 r/m=101
// - Base present:
//   - If disp==0 and base!=ebp: mod=00, no disp
//   - If disp fits int8: mod=01, disp8
//   - Else: mod=10, disp32
// - ESP base or any index => need SIB
static void encode_modrm_sib_disp(Line *L, int reg, MemAddr *m){
    // disp32 only addressing (absolute)
    if(m->base==-1 && m->index==-1){
        emit_u8(L, (uint8_t)( (0<<6) | ((reg&7)<<3) | 5 ));
        emit_u32(L, (uint32_t)m->disp);
        return;
    }

    int need_sib = (m->base==4) || (m->index!=-1);
    int32_t disp = m->disp;
    int mod = 0;
    int rm = 0;

    if(disp==0 && m->base!=5){ // base!=ebp
        mod = 0;
    } else if(disp >= -128 && disp <= 127){
        mod = 1; // disp8
    } else {
        mod = 2; // disp32
    }

    if(need_sib){
        rm = 4; // SIB follows
    } else {
        rm = m->base & 7;
        if(m->base==5 && mod==0){
            mod = 1; // disp8
            disp = 0;
        }
    }

    emit_u8(L, (uint8_t)((mod<<6)|((reg&7)<<3)|(rm&7)));

    if(need_sib){
        int ss = 0;
        if(m->scale==2) ss=1;
        else if(m->scale==4) ss=2;
        else if(m->scale==8) ss=3;
        else ss=0;

        int index = (m->index==-1?4:m->index); // 4=no index
        int base  = (m->base==-1?5:m->base);   // 5=disp32 when mod==00

        // If base==-1 and SIB used, ensure disp32-only addressing via base=5
        if(m->base==-1 && m->index!=-1 && mod==0){
            base = 5;
        }

        emit_u8(L, (uint8_t)(((ss&3)<<6)|((index&7)<<3)|(base&7)));
    }

    if(mod==1){
        emit_u8(L, (uint8_t)(disp & 0xFF));
    } else if(mod==2 || (m->base==-1 && m->index==-1)){
        emit_u32(L, (uint32_t)disp);
    }
}

static void encode_reg_rm(Line *L, uint8_t opcode, int reg, Operand *rm){
    emit_u8(L, opcode);
    if(rm->kind==OP_REG){
        uint8_t modrm = (uint8_t)( (3<<6) | ((reg&7)<<3) | (rm->reg&7) );
        emit_u8(L, modrm);
    } else if(rm->kind==OP_MEM){
        encode_modrm_sib_disp(L, reg, &rm->mem);
    }
}

static void encode_imm32_rm(Line *L, uint8_t opcode, int ext, Operand *rm, int32_t imm){
    emit_u8(L, opcode); // typically 0x81
    if(rm->kind==OP_REG){
        uint8_t modrm = (uint8_t)( (3<<6) | ((ext&7)<<3) | (rm->reg&7) );
        emit_u8(L, modrm);
    } else {
        encode_modrm_sib_disp(L, ext, &rm->mem);
    }
    emit_u32(L, (uint32_t)imm);
}

static void encode_mov(Line *L, Operand *dst, Operand *src){
    if(dst->kind==OP_REG && src->kind==OP_IMM){
        emit_u8(L, (uint8_t)(0xB8 + dst->reg));
        emit_u32(L, (uint32_t)src->imm);
    } else if(dst->kind==OP_REG && src->kind==OP_REG){
        // mov r/m32, r32 (using reg form with r/m=reg)
        encode_reg_rm(L, 0x89, src->reg, dst);
    } else if(dst->kind==OP_REG && src->kind==OP_MEM){
        // mov r32, r/m32
        encode_reg_rm(L, 0x8B, dst->reg, src);
    } else if(dst->kind==OP_MEM && src->kind==OP_REG){
        // mov r/m32, r32
        encode_reg_rm(L, 0x89, src->reg, dst);
    } else if(dst->kind==OP_MEM && src->kind==OP_IMM){
        // mov r/m32, imm32 : C7 /0 imm32
        emit_u8(L, 0xC7);
        encode_modrm_sib_disp(L, 0, &dst->mem); // /0
        emit_u32(L, (uint32_t)src->imm);
    }
}


static void encode_group1(Line *L, uint8_t opc_rm_reg, uint8_t opc_reg_rm, int ext_imm,
                          Operand *dst, Operand *src){
    if(src->kind==OP_REG){
        if(dst->kind==OP_MEM || dst->kind==OP_REG){
            encode_reg_rm(L, opc_rm_reg, src->reg, dst);
        }
    } else if(src->kind==OP_MEM){
        if(dst->kind==OP_REG){
            encode_reg_rm(L, opc_reg_rm, dst->reg, src);
        }
    } 
    
    else if(src->kind==OP_IMM){
    // Special case: eax, imm32
    if(dst->kind==OP_REG && dst->reg==0){
        // If imm fits in signed 8-bit, use 83 /0 ib
        if(src->imm >= -128 && src->imm <= 127){
            emit_u8(L, 0x83);
            uint8_t modrm = (uint8_t)( (3<<6) | ((ext_imm&7)<<3) | (dst->reg&7) );
            emit_u8(L, modrm);
            emit_u8(L, (uint8_t)(src->imm & 0xFF));
            return;
        }
        // Otherwise use special imm32 opcodes
        uint8_t special=0;
        if(ext_imm==0) special=0x05;   // add eax, imm32
        else if(ext_imm==5) special=0x2D; // sub eax, imm32
        else if(ext_imm==7) special=0x3D; // cmp eax, imm32
        if(special){
            emit_u8(L, special);
            emit_u32(L, (uint32_t)src->imm);
            return;
        }
    }
    // Generic r/m32, imm
    if(src->imm >= -128 && src->imm <= 127){
        emit_u8(L, 0x83);
        if(dst->kind==OP_REG){
            uint8_t modrm = (uint8_t)( (3<<6) | ((ext_imm&7)<<3) | (dst->reg&7) );
            emit_u8(L, modrm);
        } else {
            encode_modrm_sib_disp(L, ext_imm, &dst->mem);
        }
        emit_u8(L, (uint8_t)(src->imm & 0xFF));
    } else {
        encode_imm32_rm(L, 0x81, ext_imm, dst, src->imm);
    }
}

}


static void encode_inc_dec(Line *L, int is_inc, Operand *op){
    if(op->kind==OP_REG){
        emit_u8(L, (uint8_t)((is_inc?0x40:0x48) + op->reg));
    } else if(op->kind==OP_MEM){
        emit_u8(L, 0xFF);
        int ext = is_inc?0:1;
        encode_modrm_sib_disp(L, ext, &op->mem);
    }
}

static void encode_mul_div(Line *L, int is_mul, Operand *op){
    // mul r/m32: F7 /4 ; div r/m32: F7 /6
    emit_u8(L, 0xF7);
    int ext = is_mul?4:6;
    if(op->kind==OP_REG){
        uint8_t modrm = (uint8_t)( (3<<6) | ((ext&7)<<3) | (op->reg&7) );
        emit_u8(L, modrm);
    } else if(op->kind==OP_MEM){
        encode_modrm_sib_disp(L, ext, &op->mem);
    }
}

static void encode_cmp(Line *L, Operand *a, Operand *b){
    // cmp: r/m32, r32 = 0x39; r32, r/m32 = 0x3B; imm = /7
    encode_group1(L, 0x39, 0x3B, 7, a, b);
}

static void encode_add(Line *L, Operand *a, Operand *b){
    // add: r/m32, r32 = 0x01; r32, r/m32 = 0x03; imm = /0
    encode_group1(L, 0x01, 0x03, 0, a, b);
}

static void encode_sub(Line *L, Operand *a, Operand *b){
    // sub: r/m32, r32 = 0x29; r32, r/m32 = 0x2B; imm = /5
    encode_group1(L, 0x29, 0x2B, 5, a, b);
}

static void encode_je(Line *L, const char *label){
    // near Jcc: 0F 84 rel32
    emit_u8(L, 0x0F); emit_u8(L, 0x84);
    emit_u32(L, 0); // placeholder, fixup in second pass
    strncat(L->text, "", sizeof(L->text)-1);
    strncat(L->text, label, sizeof(L->text)-1);

    }

static void record_line(const char *src){
    if(nlines>=MAX_LINES) return;
    strncpy(lines[nlines].text, src, sizeof(lines[nlines].text)-1);
    lines[nlines].text[sizeof(lines[nlines].text)-1] = 0;
    lines[nlines].sec = cursec;
    lines[nlines].blen = 0;
    nlines++;
}


static void assemble_line(Line *L){
    // set starting offset for this line
    if(L->sec==SEC_DATA) L->off = off_data;
    else if(L->sec==SEC_BSS) L->off = off_bss;
    else if(L->sec==SEC_TEXT) L->off = off_text;

    char buf[256]; strncpy(buf, L->text, sizeof(buf)-1); buf[sizeof(buf)-1]=0;
    char *semi = strchr(buf,';'); if(semi) *semi=0;
    trim(buf);
    if(!*buf) return;

    // section directives
    if(strncmp(buf,"section",7)==0){
        if(strstr(buf,".data")) cursec=SEC_DATA;
        else if(strstr(buf,".bss")) cursec=SEC_BSS;
        else if(strstr(buf,".text")) cursec=SEC_TEXT;
        return;
    }
    if(strncmp(buf,"global",6)==0) { /* ignore in listing */ return; }

    // label definition
    char *colon = strchr(buf,':');
    if(colon){
        *colon=0; trim(buf);
        add_symbol(buf);
        // rest after colon
        char *rest = colon+1; trim(rest);
        if(!*rest) return;
        strncpy(buf, rest, sizeof(buf)-1); buf[sizeof(buf)-1]=0;
    }

    // data directives
    if(strncmp(buf,"value",5)==0 || strncmp(buf,"dd",2)==0 || strstr(buf," dd ")){
        // e.g., value1 dd 11, 12
        char *dd = strstr(buf,"dd");
        if(!dd) return;
        char *p = dd+2;
        while(*p){
            while(isspace((unsigned char)*p)) p++;
            int sign=+1; if(*p=='-'){sign=-1; p++;}
            char num[32]; int k=0;
            while(isxdigit((unsigned char)*p)) { num[k++]=*p++; if(k>=31)break; }
            num[k]=0; if(k>0){
                int32_t v=0; parse_int(num,&v);
                emit_u32(L,(uint32_t)v);
                off_data += 4;
            }
            while(isspace((unsigned char)*p)) p++;
            if(*p==',') p++;
        }
        return;
    }
    if(strstr(buf,"resd")){
        int32_t n=0; char *p=strstr(buf,"resd")+4; while(isspace((unsigned char)*p)) p++;
        parse_int(p,&n); off_bss += (uint32_t)(n*4); return;
    }
    if(strstr(buf,"resb")){
        int32_t n=0; char *p=strstr(buf,"resb")+4; while(isspace((unsigned char)*p)) p++;
        parse_int(p,&n); off_bss += (uint32_t)n; return;
    }

    // instruction parsing
    char *tok[MAX_TOKENS]; int nt = tokenize(buf,tok,MAX_TOKENS);
    if(nt<=0) return;

    const char *mn = tok[0];
    int i=1;

    // single-operand instructions
    if(strcmp(mn,"ret")==0){
        emit_u8(L, 0xC3);
        if(L->sec==SEC_TEXT) off_text += L->blen;
        goto done;
    }
    if(strcmp(mn,"inc")==0 || strcmp(mn,"dec")==0){
        Operand op; if(!parse_operand(tok,nt,&i,&op)) goto done;
        encode_inc_dec(L, strcmp(mn,"inc")==0, &op);
        if(L->sec==SEC_TEXT) off_text += L->blen;
        goto done;
    }
    if(strcmp(mn,"mul")==0 || strcmp(mn,"div")==0){
        Operand op; if(!parse_operand(tok,nt,&i,&op)) goto done;
        encode_mul_div(L, strcmp(mn,"mul")==0, &op);
        if(L->sec==SEC_TEXT) off_text += L->blen;
        goto done;
    }
    if(strcmp(mn,"je")==0){
        // je label
        if(i<nt){
            encode_je(L, tok[i]);
            if(L->sec==SEC_TEXT) off_text += L->blen;
        }
        goto done;
    }

    // two-operand instructions
    Operand dst, src;
    if(!parse_operand(tok,nt,&i,&dst)) goto done;
    if(i<nt && strcmp(tok[i],",")==0) i++;
    if(!parse_operand(tok,nt,&i,&src)) goto done;

    if(strcmp(mn,"mov")==0){
        encode_mov(L,&dst,&src);
    } else if(strcmp(mn,"add")==0){
        encode_add(L,&dst,&src);
    } else if(strcmp(mn,"sub")==0){
        encode_sub(L,&dst,&src);
    } else if(strcmp(mn,"cmp")==0){
        encode_cmp(L,&dst,&src);
    }

    if(L->sec==SEC_TEXT) off_text += L->blen;
done:
    for(int k=0;k<nt;k++) free(tok[k]);
}

static void patch_branches(){
    // Patch JE rel32 for lines that have "@@label"
    for(int li=0; li<nlines; li++){
        Line *L = &lines[li];
        if(L->sec!=SEC_TEXT || L->blen<6) continue;
        char *at = strstr(L->text,"@@");
        if(!at) continue;
        const char *label = at+2;
        const Symbol *S = find_symbol(label);
        if(!S || S->sec!=SEC_TEXT) continue;
        //uint32_t src = L->off + 6; // end of instruction
	uint32_t src = L->off + L->blen; // end of instruction
        uint32_t dst = S->off;
        int32_t rel = (int32_t)((int64_t)dst - (int64_t)src);
        // bytes: 0F 84 <rel32>
        L->bytes[2] = (uint8_t)(rel & 0xFF);
        L->bytes[3] = (uint8_t)((rel >> 8) & 0xFF);
        L->bytes[4] = (uint8_t)((rel >> 16) & 0xFF);
        L->bytes[5] = (uint8_t)((rel >> 24) & 0xFF);
    }
}

static void print_listing(){
    // Data section
    printf("section .data\n");
    uint32_t off=0;
    for(int i=0;i<nlines;i++){
        if(lines[i].sec!=SEC_DATA) continue;
        if(lines[i].blen==0) continue;
        //printf("%-10u ", off);
	printf("%08X ", off);
        for(int b=0;b<lines[i].blen;b++) printf("%02X", lines[i].bytes[b]);
        printf("        %s\n", lines[i].text);
        off += lines[i].blen;
    }
    // BSS section
    printf("\nsection .bss\n");
    //printf("%-10u <res %Xh>\n", 0u, off_bss); // simplistic summary
    printf("%08X <res %08Xh>\n", 0u, off_bss);

    // Text section
    printf("\nsection .text\n");
    for(int i=0;i<nlines;i++){
        if(lines[i].sec!=SEC_TEXT) continue;
        //printf("%-10u ", lines[i].off);
	printf("%08X ", lines[i].off);

        for(int b=0;b<lines[i].blen;b++) printf("%02X", lines[i].bytes[b]);
        printf("        %s\n", lines[i].text);
    }
}

int main(int argc, char **argv){
    if(argc<2){ fprintf(stderr,"usage: %s input.asm\n", argv[0]); return 1; }
    FILE *f = fopen(argv[1],"r");
    if(!f){ perror("open"); return 1; }
    char line[256];
    while(fgets(line,sizeof(line),f)){
        // store raw line for second pass, but also collect labels and sections
        char tmp[256]; strncpy(tmp,line,sizeof(tmp)-1); tmp[sizeof(tmp)-1]=0;
        char *semi = strchr(tmp,';'); if(semi) *semi=0;
        trim(tmp);
        if(!*tmp){ record_line(line); continue; }
        if(strncmp(tmp,"section",7)==0){
            if(strstr(tmp,".data")) cursec=SEC_DATA;
            else if(strstr(tmp,".bss")) cursec=SEC_BSS;
            else if(strstr(tmp,".text")) cursec=SEC_TEXT;
            record_line(line); continue;
        }
        char *colon = strchr(tmp,':');
        if(colon){
            *colon=0; trim(tmp); add_symbol(tmp);
        }
        record_line(line);
    }
    fclose(f);

    // Second pass: assemble with offsets
    cursec = SEC_NONE; off_data=0; off_bss=0; off_text=0;
    for(int i=0;i<nlines;i++) assemble_line(&lines[i]);
    patch_branches();
    print_listing();
    return 0;
}

