#include "all.h"
#include "config.h"
#include <ctype.h>
#include <getopt.h>

Target T;

char debug['Z'+1] = {
    ['9'] = 0, /* debug all */
	['P'] = 0, /* parsing */
	['M'] = 0, /* memory optimization */
	['N'] = 0, /* ssa construction */
	['C'] = 0, /* copy elimination */
	['F'] = 0, /* constant folding */
	['A'] = 0, /* abi lowering */
	['I'] = 0, /* instruction selection */
	['L'] = 0, /* liveness */
	['S'] = 0, /* spilling */
	['R'] = 0, /* reg. allocation */
};

extern Target T_amd64_sysv;
extern Target T_amd64_apple;
extern Target T_arm64;
extern Target T_arm64_apple;
extern Target T_rv64;

static Target *tlist[] = {
	&T_amd64_sysv,
	&T_amd64_apple,
	&T_arm64,
	&T_arm64_apple,
	&T_rv64,
	0
};
static FILE *outf;
static int dbg;

static void
data(Dat *d)
{
	if (dbg)
		return;
	emitdat(d, outf);
	if (d->type == DEnd) {
		fputs("/* end data */\n\n", outf);
		freeall();
	}
}

/// TODO ???
static void
func(Fn *fn)
{
	uint n;

	if (dbg)
		fprintf(stderr, "**** Function %s ****", fn->name);
	if (debug['P']) {
		fprintf(stderr, "\n> After parsing:\n");
		printfn(fn, stderr);
	}
	T.abi0(fn);     if(debug['9']) { fprintf(stderr, "\n> After abi0:\n"); printfn(fn, stderr); }
	fillrpo(fn);    if(debug['9']) { fprintf(stderr, "\n> After fillrpo:\n"); printfn(fn, stderr); }
	fillpreds(fn);  if(debug['9']) { fprintf(stderr, "\n> After fillpreds:\n"); printfn(fn, stderr); }
	filluse(fn);    if(debug['9']) { fprintf(stderr, "\n> After filluse:\n"); printfn(fn, stderr); }
	promote(fn);    if(debug['9']) { fprintf(stderr, "\n> After promote:\n"); printfn(fn, stderr); }
	filluse(fn);    if(debug['9']) { fprintf(stderr, "\n> After filluse:\n"); printfn(fn, stderr); }
	ssa(fn);        if(debug['9']) { fprintf(stderr, "\n> After ssa:\n"); printfn(fn, stderr); }
	filluse(fn);    if(debug['9']) { fprintf(stderr, "\n> After filluse:\n"); printfn(fn, stderr); }
	ssacheck(fn);   if(debug['9']) { fprintf(stderr, "\n> After ssacheck:\n"); printfn(fn, stderr); }
	fillalias(fn);  if(debug['9']) { fprintf(stderr, "\n> After fillalias:\n"); printfn(fn, stderr); }
	loadopt(fn);    if(debug['9']) { fprintf(stderr, "\n> After loadopt:\n"); printfn(fn, stderr); }
	filluse(fn);    if(debug['9']) { fprintf(stderr, "\n> After filluse:\n"); printfn(fn, stderr); }
	fillalias(fn);  if(debug['9']) { fprintf(stderr, "\n> After fillalias:\n"); printfn(fn, stderr); }
	coalesce(fn);   if(debug['9']) { fprintf(stderr, "\n> After coalesce:\n"); printfn(fn, stderr); }
	filluse(fn);    if(debug['9']) { fprintf(stderr, "\n> After filluse:\n"); printfn(fn, stderr); }
	ssacheck(fn);   if(debug['9']) { fprintf(stderr, "\n> After ssacheck:\n"); printfn(fn, stderr); }
	copy(fn);       if(debug['9']) { fprintf(stderr, "\n> After copy:\n"); printfn(fn, stderr); }
	filluse(fn);    if(debug['9']) { fprintf(stderr, "\n> After filluse:\n"); printfn(fn, stderr); }
	fold(fn);       if(debug['9']) { fprintf(stderr, "\n> After fold:\n"); printfn(fn, stderr); }
	T.abi1(fn);     if(debug['9']) { fprintf(stderr, "\n> After abi1:\n"); printfn(fn, stderr); }
	simpl(fn);      if(debug['9']) { fprintf(stderr, "\n> After simpl:\n"); printfn(fn, stderr); }
	fillpreds(fn);  if(debug['9']) { fprintf(stderr, "\n> After fillpreds:\n"); printfn(fn, stderr); }
	filluse(fn);    if(debug['9']) { fprintf(stderr, "\n> After filluse:\n"); printfn(fn, stderr); }
	T.isel(fn);     if(debug['9']) { fprintf(stderr, "\n> After isel:\n"); printfn(fn, stderr); }
	fillrpo(fn);    if(debug['9']) { fprintf(stderr, "\n> After fillrpo:\n"); printfn(fn, stderr); }
	filllive(fn);   if(debug['9']) { fprintf(stderr, "\n> After filllive:\n"); printfn(fn, stderr); }
	fillloop(fn);   if(debug['9']) { fprintf(stderr, "\n> After fillloop:\n"); printfn(fn, stderr); }
	fillcost(fn);   if(debug['9']) { fprintf(stderr, "\n> After fillcost:\n"); printfn(fn, stderr); }
	spill(fn);      if(debug['9']) { fprintf(stderr, "\n> After spill:\n"); printfn(fn, stderr); }
	rega(fn);       if(debug['9']) { fprintf(stderr, "\n> After rega:\n"); printfn(fn, stderr); }
	fillrpo(fn);    if(debug['9']) { fprintf(stderr, "\n> After fillrpo:\n"); printfn(fn, stderr); }
	simpljmp(fn);   if(debug['9']) { fprintf(stderr, "\n> After simpljmp:\n"); printfn(fn, stderr); }
	fillpreds(fn);  if(debug['9']) { fprintf(stderr, "\n> After fillpreds:\n"); printfn(fn, stderr); }
	fillrpo(fn);    if(debug['9']) { fprintf(stderr, "\n> After fillrpo:\n"); printfn(fn, stderr); }
	assert(fn->rpo[0] == fn->start);
	for (n=0;; n++)
		if (n == fn->nblk-1) {
			fn->rpo[n]->link = 0;
			break;
		} else
			fn->rpo[n]->link = fn->rpo[n+1];
	if (!dbg) {
		T.emitfn(fn, outf);
		fprintf(outf, "/* end function %s */\n\n", fn->name);
	} else
		fprintf(stderr, "\n");
	freeall();
}

static void
dbgfile(char *fn)
{
	emitdbgfile(fn, outf);
}

int
main(int ac, char *av[])
{
	Target **t;
	FILE *inf, *hf;
	char *f, *sep;
	int c;

	T = Deftgt;
	outf = stdout;
	while ((c = getopt(ac, av, "hd:o:t:")) != -1)
		switch (c) {
		case 'd':
			for (; *optarg; optarg++)
				if (isalpha(*optarg)) {
					debug[toupper(*optarg)] = 1;
					dbg = 1;
				}
                else if (*optarg == '9') {
                    debug[*optarg] = 1;
                    dbg = 1;
                }
			break;
		case 'o':
			if (strcmp(optarg, "-") != 0) {
				outf = fopen(optarg, "w");
				if (!outf) {
					fprintf(stderr, "cannot open '%s'\n", optarg);
					exit(1);
				}
			}
			break;
		case 't':
			if (strcmp(optarg, "?") == 0) {
				puts(T.name);
				exit(0);
			}
			for (t=tlist;; t++) {
				if (!*t) {
					fprintf(stderr, "unknown target '%s'\n", optarg);
					exit(1);
				}
				if (strcmp(optarg, (*t)->name) == 0) {
					T = **t;
					break;
				}
			}
			break;
		case 'h':
		default:
			hf = c != 'h' ? stderr : stdout;
			fprintf(hf, "%s [OPTIONS] {file.ssa, -}\n", av[0]);
			fprintf(hf, "\t%-11s prints this help\n", "-h");
			fprintf(hf, "\t%-11s output to file\n", "-o file");
			fprintf(hf, "\t%-11s generate for a target among:\n", "-t <target>");
			fprintf(hf, "\t%-11s ", "");
			for (t=tlist, sep=""; *t; t++, sep=", ") {
				fprintf(hf, "%s%s", sep, (*t)->name);
				if (*t == &Deftgt)
					fputs(" (default)", hf);
			}
			fprintf(hf, "\n");
			fprintf(hf, "\t%-11s dump debug information\n", "-d <flags>");
			exit(c != 'h');
		}

	do {
		f = av[optind];
		if (!f || strcmp(f, "-") == 0) {
			inf = stdin;
			f = "-";
		} else {
			inf = fopen(f, "r");
			if (!inf) {
				fprintf(stderr, "cannot open '%s'\n", f);
				exit(1);
			}
		}
		parse(inf, f, dbgfile, data, func);
		fclose(inf);
	} while (++optind < ac);

	if (!dbg)
		T.emitfin(outf);

	exit(0);
}
