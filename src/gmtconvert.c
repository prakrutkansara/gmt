/*--------------------------------------------------------------------
 *	$Id$
 *
 *	Copyright (c) 1991-2016 by P. Wessel, W. H. F. Smith, R. Scharroo, J. Luis and F. Wobbe
 *	See LICENSE.TXT file for copying and redistribution conditions.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU Lesser General Public License as published by
 *	the Free Software Foundation; version 3 or any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU Lesser General Public License for more details.
 *
 *	Contact info: gmt.soest.hawaii.edu
 *--------------------------------------------------------------------*/
/*
 * API functions to support the gmtconvert application.
 *
 * Author:	Paul Wessel
 * Date:	1-JAN-2010
 * Version:	5 API
 *
 * Brief synopsis: Read one or more data tables and can concatenated them
 * vertically [Default] or horizontally (pasting), select certain columns,
 * report only first and/or last record per segment, only print segment
 * headers, and only report segments that passes a segment header search.
 */

#define THIS_MODULE_NAME	"gmtconvert"
#define THIS_MODULE_LIB		"core"
#define THIS_MODULE_PURPOSE	"Convert, paste, or extract columns from data tables"
#define THIS_MODULE_KEYS	"<D{,>D}"

#include "gmt_dev.h"

#define GMT_PROG_OPTIONS "-:>Vabdfghios" GMT_OPT("HMm")

EXTERN_MSC int gmt_get_ogr_id (struct GMT_OGR *G, char *name);
EXTERN_MSC int gmt_parse_o_option (struct GMT_CTRL *GMT, char *arg);

#define INV_ROWS	1
#define INV_SEGS	2
#define INV_TBLS	4

/* Control structure for gmtconvert */

struct GMTCONVERT_CTRL {
	struct Out {	/* -> */
		bool active;
		char *file;
	} Out;
	struct A {	/* -A */
		bool active;
	} A;
	struct C {	/* -C[+l<min>+u<max>+i>] */
		bool active, invert;
		uint64_t min, max;
	} C;
	struct D {	/* -D[<template>][+o<orig>] */
		bool active;
		bool origin;
		unsigned int mode;
		unsigned int t_orig, s_orig;
		char *name;
	} D;
	struct E {	/* -E */
		bool active;
		int mode;	/* -3, -1, -1, 0, or increment stride */
	} E;
	struct F {	/* -F<mode> */
		bool active;
		struct GMT_SEGMENTIZE S;
	} F;
	struct L {	/* -L */
		bool active;
	} L;
	struct I {	/* -I[ast] */
		bool active;
		unsigned int mode;
	} I;
	struct Q {	/* -Q<selections> */
		bool active;
		struct GMT_INT_SELECTION *select;
	} Q;
	struct S {	/* -S[~]\"search string\" */
		bool active;
		struct GMT_TEXT_SELECTION *select;
	} S;
	struct T {	/* -T */
		bool active;
	} T;
};

GMT_LOCAL void *New_Ctrl (struct GMT_CTRL *GMT) {	/* Allocate and initialize a new control structure */
	struct GMTCONVERT_CTRL *C;

	C = gmt_M_memory (GMT, NULL, 1, struct GMTCONVERT_CTRL);

	/* Initialize values whose defaults are not 0/false/NULL */
	C->C.max = ULONG_MAX;	/* Max records possible in one segment */

	return (C);
}

GMT_LOCAL void Free_Ctrl (struct GMT_CTRL *GMT, struct GMTCONVERT_CTRL *C) {	/* Deallocate control structure */
	if (!C) return;
	gmt_M_str_free (C->Out.file);
	gmt_M_str_free (C->D.name);
	if (C->S.active) gmt_free_text_selection (GMT, &C->S.select);
	if (C->Q.active) gmt_free_int_selection (GMT, &C->Q.select);
	gmt_M_free (GMT, C);
}

GMT_LOCAL int usage (struct GMTAPI_CTRL *API, int level) {
	gmt_show_name_and_purpose (API, THIS_MODULE_LIB, THIS_MODULE_NAME, THIS_MODULE_PURPOSE);
	if (level == GMT_MODULE_PURPOSE) return (GMT_NOERROR);
	GMT_Message (API, GMT_TIME_NONE, "usage: gmtconvert [<table>] [-A] [-C[+l<min>][+u<max>][+i]] [-D[<template>[+o<orig>]]] [-E[f|l|m<stride>]] [-F<arg>] [-I[tsr]]\n");
	GMT_Message (API, GMT_TIME_NONE, "\t[-L] [-Q[~]<selection>] [-S[~]\"search string\"] [-T] [%s] [%s]\n\t[%s] [%s] [%s] [%s]\n", GMT_V_OPT, GMT_a_OPT, GMT_b_OPT, GMT_d_OPT, GMT_f_OPT, GMT_g_OPT);
	GMT_Message (API, GMT_TIME_NONE, "\t[%s] [%s]\n\t[%s] [%s] [%s]\n\n", GMT_h_OPT, GMT_i_OPT, GMT_o_OPT, GMT_s_OPT, GMT_colon_OPT);

	if (level == GMT_SYNOPSIS) return (GMT_MODULE_SYNOPSIS);

	GMT_Message (API, GMT_TIME_NONE, "\n\tOPTIONS:\n");
	GMT_Option (API, "<");
	GMT_Message (API, GMT_TIME_NONE, "\t-A Paste files horizontally, not concatenate vertically [Default].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   All files must have the same number of segments and rows,\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   but they may differ in their number of columns.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t-C Only output segments whose number of records matches criteria:\n");
	GMT_Message (API, GMT_TIME_NONE, "\t     +l<min> Segment must have at least <min> records [0].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t     +u<max> Segment must have at most <max> records [inf].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t     +i will invert the test.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t-D Write individual segments to separate files [Default writes one\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   multisegment file to stdout].  Append file name template which MUST\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   contain a C-style format for an integer (e.g., %%d) that represents\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   a sequential segment number across all tables (if more than one table).\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   [Default uses gmtconvert_segment_%%d.txt (or .bin for binary)].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   Use +o<orig> to start numbering at <orig> [0].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   Alternatively, supply a template with two long formats and we will\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   replace them with the table number and table segment numbers.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   Use +o<t_orig>/<s_orig> to start numbering at <t_orig> for tables and <s_orig> for segments [0/0].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t-E Extract first and last point per segment only [Output all points].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   Append f for first only or l for last only.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   Append m<stride> to pass only 1 out of <stride> records.\n");
	gmt_segmentize_syntax (API->GMT, 'F', 0);
	GMT_Message (API, GMT_TIME_NONE, "\t-I Invert output order of (t)ables, (s)egments, or (r)ecords.  Append any combination of:\n");
	GMT_Message (API, GMT_TIME_NONE, "\t     t: reverse the order of input tables on output.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t     s: reverse the order of segments within each table on output.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t     r: reverse the order of records within each segment on output [Default].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t-L Output only segment headers and skip all data records.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   Requires ASCII input data [Output headers and data].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t-Q Only output specified segment numbers in <selection> [All].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   <selection> syntax is [~]<range>[,<range>,...] where each <range> of items is\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   either a single number, start-stop (for range), start:step:stop (for stepped range),\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   or +f<file> for a file list with one <range> selection per line.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   A leading ~ will invert the selection and write all segments but the ones listed.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t-S Only output segments whose headers contain the pattern \"string\".\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   Use -S~\"string\" to output segment that DO NOT contain this pattern.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   If your pattern begins with ~, escape it with \\~.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   To match OGR aspatial values, use name=value.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   To match against extended regular expressions use -S[~]/regexp/[i].\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   Give +f<file> for a file list with such patterns, one per line.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t   To give a single pattern starting with +f, escape it with \\+f.\n");
	GMT_Message (API, GMT_TIME_NONE, "\t-T Prevent the writing of segment headers.\n");
	GMT_Option (API, "V,a,bi,bo,d,f,g,h,i,o,s,:,.");

	return (GMT_MODULE_USAGE);
}

GMT_LOCAL int parse (struct GMT_CTRL *GMT, struct GMTCONVERT_CTRL *Ctrl, struct GMT_OPTION *options) {
	/* This parses the options provided to gmtconvert and sets parameters in CTRL.
	 * Any GMT common options will override values set previously by other commands.
	 * It also replaces any file names specified as input or output with the data ID
	 * returned when registering these sources/destinations with the API.
	 */

	unsigned int pos, n_errors = 0, k, n_files = 0;
	int n = 0;
	int64_t value = 0;
	char p[GMT_BUFSIZ] = {""}, *c = NULL;
	struct GMT_OPTION *opt = NULL;
	struct GMTAPI_CTRL *API = GMT->parent;

	for (opt = options; opt; opt = opt->next) {
		switch (opt->option) {

			case '<':	/* Skip input files */
				if (!gmt_check_filearg (GMT, '<', opt->arg, GMT_IN, GMT_IS_DATASET)) n_errors++;
				break;
			case '>':	/* Got named output file */
				if (n_files++ == 0 && gmt_check_filearg (GMT, '>', opt->arg, GMT_OUT, GMT_IS_DATASET))
					Ctrl->Out.file = strdup (opt->arg);
				else
					n_errors++;
				break;

			/* Processes program-specific parameters */

			case 'A':	/* pAste mode */
				Ctrl->A.active = true;
				break;
			case 'C':	/* record-count selection mode */
				Ctrl->C.active = true;
				pos = 0;
				while (gmt_getmodopt (GMT, opt->arg, "ilu", &pos, p)) {	/* Looking for +i, +l, +u */
					switch (p[0]) {
						case 'i':	/* Invert selection */
							Ctrl->C.invert = true;	break;
						case 'l':	/* Set fewest records required */
						 	if ((value = atol (&p[1])) < 0)
								GMT_Report (API, GMT_MSG_NORMAL, "Error: The -C+l modifier was given negative record count!\n");
							else
								Ctrl->C.min = (uint64_t)value;
							break;
						case 'u':	/* Set max records required */
					 		if ((value = atol (&p[1])) < 0)
								GMT_Report (API, GMT_MSG_NORMAL, "Error: The -C+u modifier was given negative record count!\n");
							else
								Ctrl->C.max = (uint64_t)value;
							break;
						default:
							n_errors++;	break;
					}
				}
				break;
			case 'D':	/* Write each segment to a separate output file */
				Ctrl->D.active = true;
				if ((c = strstr (opt->arg, "+o"))) {	/* Gave new origins for tables and segments (or just segments) */
					n = sscanf (&c[2], "%d/%d", &Ctrl->D.t_orig, &Ctrl->D.s_orig);
					if (n == 1) Ctrl->D.s_orig = Ctrl->D.t_orig, Ctrl->D.t_orig = 0;
					c[0] = '\0';	/* Chop off modifier */
					Ctrl->D.origin = true;
				}
				if (*opt->arg) /* optarg is optional */
					Ctrl->D.name = strdup (opt->arg);
				if (c) c[0] = '+';	/* Restore modifier */
				break;
			case 'E':	/* Extract ends only */
				Ctrl->E.active = true;
				switch (opt->arg[0]) {
					case 'f':		/* Get first point only */
						Ctrl->E.mode = -1; break;
					case 'l':		/* Get last point only */
						Ctrl->E.mode = -2; break;
					case 'm':		/* Set modulo step */
						Ctrl->E.mode = atoi (&opt->arg[1]); break;
					default:		/* Get first and last point only */
						Ctrl->E.mode = -3; break;
				}
				break;
			case 'F':
				Ctrl->F.active = true;
				if (opt->arg[0] == '\0') {	/* No arguments, must be old GMT4 option -F */
					if (gmt_M_compat_check (GMT, 4)) {
						GMT_Report (API, GMT_MSG_COMPAT,
						            "Warning: Option -F for output columns is deprecated; use -o instead\n");
						gmt_parse_o_option (GMT, opt->arg);
					}
					else
						n_errors += gmt_default_error (GMT, opt->option);
					break;
				}
				/* Modern options */
				n_errors += gmt_parse_segmentize (GMT, opt->option, opt->arg, 0, &(Ctrl->F.S));
				break;
			case 'I':	/* Invert order or tables, segments, rows as indicated */
				Ctrl->I.active = true;
				for (k = 0; opt->arg[k]; k++) {
					switch (opt->arg[k]) {
						case 't': Ctrl->I.mode |= INV_TBLS; break;	/* Reverse table order */
						case 's': Ctrl->I.mode |= INV_SEGS; break;	/* Reverse segment order */
						case 'r': Ctrl->I.mode |= INV_ROWS; break;	/* Reverse record order */
						default:
							GMT_Report (API, GMT_MSG_NORMAL,
							            "Error: The -I option does not recognize modifier %c\n", (int)opt->arg[k]);
							n_errors++;
							break;
					}
				}
				if (Ctrl->I.mode == 0) Ctrl->I.mode = INV_ROWS;	/* Default is -Ir */
				break;
			case 'L':	/* Only output segment headers */
				Ctrl->L.active = true;
				break;
			case 'Q':	/* Only report for specified segment numbers */
				Ctrl->Q.active = true;
				Ctrl->Q.select = gmt_set_int_selection (GMT, opt->arg);
				break;
			case 'S':	/* Segment header pattern search */
				Ctrl->S.active = true;
				Ctrl->S.select = gmt_set_text_selection (GMT, opt->arg);
				break;
			case 'T':	/* Do not write segment headers */
				Ctrl->T.active = true;
				break;

			default:	/* Report bad options */
				n_errors += gmt_default_error (GMT, opt->option);
				break;
		}
	}

	if (Ctrl->D.active) {	/* Validate the name template, if given */
		/* Must write individual segments to separate files so create the needed name template */
		unsigned int n_formats = 0;
		if (!Ctrl->D.name) {	/* None give, auto-assign to segment files with extension based on binary or not */
			Ctrl->D.name = GMT->common.b.active[GMT_OUT] ?
				strdup ("gmtconvert_segment_%d.bin") : strdup ("gmtconvert_segment_%d.txt");
			Ctrl->D.mode = GMT_WRITE_SEGMENT;
		}
		else { /* Ctrl->D.name given, need to check correct format */
			char *p = Ctrl->D.name;
			while ( (p = strchr (p, '%')) ) {
				/* found %, now check format */
				++p;	/* Skip the % sign */
				p += strspn (p, "0123456789."); /* span past digits and decimal */
				if ( strspn (p, "diu") == 0 ) {
					/* No valid conversion specifier */
					GMT_Report (API, GMT_MSG_NORMAL,
						"Syntax error: Use of unsupported conversion specifier at position %" PRIuS " in format string '%s'.\n",
						p - Ctrl->D.name + 1, Ctrl->D.name);
					n_errors++;
				}
				++n_formats;
			}
			if ( n_formats == 0 || n_formats > 2 ) {	/* Incorrect number of format specifiers */
				GMT_Report (API, GMT_MSG_NORMAL,
					"Syntax error: Incorrect number of format specifiers in format string '%s'.\n",
					Ctrl->D.name);
				n_errors++;
			}
			/* The io_mode tells the i/o function to split tables or segments into files, if requested */
			Ctrl->D.mode = (n_formats == 2) ? GMT_WRITE_TABLE_SEGMENT: GMT_WRITE_SEGMENT;
		}
	}
	n_errors += gmt_M_check_condition (GMT, GMT->common.b.active[GMT_IN] && GMT->common.b.ncol[GMT_IN] == 0,
	                                 "Syntax error: Must specify number of columns in binary input data (-bi)\n");
	n_errors += gmt_M_check_condition (GMT, GMT->common.b.active[GMT_IN] && (Ctrl->L.active || Ctrl->S.active),
	                                 "Syntax error: -L or -S requires ASCII input data\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->C.active && (Ctrl->C.min > Ctrl->C.max),
	                                 "Syntax error: -C minimum records cannot exceed maximum records\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->D.active && Ctrl->D.name && !strstr (Ctrl->D.name, "%"),
	                                 "Syntax error: -D Output template must contain %%d\n");
	n_errors += gmt_M_check_condition (GMT, Ctrl->Q.active && Ctrl->S.active,
	                                 "Syntax error: Only one of -Q and -S can be used simultaneously\n");
	n_errors += gmt_M_check_condition (GMT, n_files > 1, "Syntax error: Only one output destination can be specified\n");

	return (n_errors ? GMT_PARSE_ERROR : GMT_NOERROR);
}

/* Must free allocated memory before returning */
#define bailout(code) {gmt_M_free_options (mode); return (code);}
#define Return(code) {Free_Ctrl (GMT, Ctrl); gmt_end_module (GMT, GMT_cpy); bailout (code);}

int GMT_gmtconvert (void *V_API, int mode, void *args) {
	bool match = false, prevent_seg_headers = false;
	int error = 0;
	uint64_t out_col, col, n_cols_in, n_cols_out, tbl;
	uint64_t n_horizontal_tbls, n_vertical_tbls, tbl_ver, tbl_hor, use_tbl;
	uint64_t last_row, n_rows, row, seg, n_out_seg = 0, out_seg = 0;

	double *val = NULL;

	char *method[2] = {"concatenated", "pasted"}, *p = NULL;

	struct GMTCONVERT_CTRL *Ctrl = NULL;
	struct GMT_DATASET *D[2] = {NULL, NULL};	/* Pointer to GMT multisegment table(s) in and out */
	struct GMT_CTRL *GMT = NULL, *GMT_cpy = NULL;
	struct GMT_OPTION *options = NULL;
	struct GMTAPI_CTRL *API = gmt_get_api_ptr (V_API);	/* Cast from void to GMTAPI_CTRL pointer */

	/*----------------------- Standard module initialization and parsing ----------------------*/

	if (API == NULL) return (GMT_NOT_A_SESSION);
	if (mode == GMT_MODULE_PURPOSE) return (usage (API, GMT_MODULE_PURPOSE));	/* Return the purpose of program */
	options = GMT_Create_Options (API, mode, args);	if (API->error) return (API->error);	/* Set or get option list */

	if (options && options->option == GMT_OPT_USAGE) bailout (usage (API, GMT_USAGE));	/* Return the usage message */
	if (options && options->option == GMT_OPT_SYNOPSIS) bailout (usage (API, GMT_SYNOPSIS));	/* Return the synopsis */

	/* Parse the command-line arguments */

	GMT = gmt_begin_module (API, THIS_MODULE_LIB, THIS_MODULE_NAME, &GMT_cpy); /* Save current state */
	if (GMT_Parse_Common (API, GMT_PROG_OPTIONS, options)) Return (API->error);
	Ctrl = New_Ctrl (GMT);	/* Allocate and initialize a new control structure */
	if ((error = parse (GMT, Ctrl, options)) != 0) Return (error);

	/*---------------------------- This is the gmtconvert main code ----------------------------*/

	GMT_Report (API, GMT_MSG_VERBOSE, "Processing input table data\n");

	if (GMT_Init_IO (API, GMT_IS_DATASET, GMT_IS_NONE, GMT_IN, GMT_ADD_DEFAULT, 0, options) != GMT_NOERROR) {
		Return (API->error);	/* Establishes data files or stdin */
	}

	/* Read in the input tables */

	if ((D[GMT_IN] = GMT_Read_Data (API, GMT_IS_DATASET, GMT_IS_FILE, 0, GMT_READ_NORMAL, NULL, NULL, NULL)) == NULL) {
		Return (API->error);
	}

	if (GMT->common.a.active && D[GMT_IN]->n_tables > 1) {
		GMT_Report (API, GMT_MSG_NORMAL, "The -a option requires a single table only.\n");
		Return (GMT_RUNTIME_ERROR);
	}
	if (GMT->common.a.active && D[GMT_IN]->table[0]->ogr) {
		GMT_Report (API, GMT_MSG_NORMAL, "The -a option requires a single table without OGR metadata.\n");
		Return (GMT_RUNTIME_ERROR);
	}

	if (Ctrl->T.active && !gmt_M_file_is_memory (Ctrl->Out.file))
		prevent_seg_headers = true;

	if (prevent_seg_headers)	/* Turn off segment headers on file output */
		GMT->current.io.skip_headers_on_outout = true;

	if (Ctrl->F.active) {	/* Segmentizing happens here and then we are done */
		D[GMT_OUT] = gmt_segmentize_data (GMT, D[GMT_IN], &(Ctrl->F.S));	/* Segmentize the data */
		if (GMT_Destroy_Data (API, &D[GMT_IN]) != GMT_NOERROR) {	/* Be gone with the original */
			Return (API->error);
		}
		if (D[GMT_OUT]->n_segments > 1) gmt_set_segmentheader (GMT, GMT_OUT, true);	/* Turn on segment headers on output */

		if (GMT_Write_Data (API, GMT_IS_DATASET, GMT_IS_FILE, D[GMT_OUT]->geometry, D[GMT_OUT]->io_mode, NULL, Ctrl->Out.file, D[GMT_OUT]) != GMT_NOERROR) {
			Return (API->error);
		}
		if (prevent_seg_headers) GMT->current.io.skip_headers_on_outout = false;	/* Restore to default if it was changed */
		Return (GMT_NOERROR);	/* We are done! */
	}

	/* Determine number of input and output columns for the selected options.
	 * For -A, require all tables to have the same number of segments and records. */

	for (tbl = n_cols_in = n_cols_out = 0; tbl < D[GMT_IN]->n_tables; tbl++) {
		if (Ctrl->A.active) {	/* All tables must be of the same vertical shape */
			if (tbl && D[GMT_IN]->table[tbl]->n_records  != D[GMT_IN]->table[tbl-1]->n_records)  error = true;
			if (tbl && D[GMT_IN]->table[tbl]->n_segments != D[GMT_IN]->table[tbl-1]->n_segments) error = true;
		}
		n_cols_in += D[GMT_IN]->table[tbl]->n_columns;				/* This is the case for -A */
		n_cols_out = MAX (n_cols_out, D[GMT_IN]->table[tbl]->n_columns);	/* The widest table encountered */
	}
	n_cols_out = (Ctrl->A.active) ? n_cols_in : n_cols_out;	/* Default or Reset since we did not have -A */

	if (error) {
		GMT_Report (API, GMT_MSG_NORMAL, "Parsing requires files with same number of records.\n");
		Return (GMT_RUNTIME_ERROR);
	}
	if (n_cols_out == 0) {
		GMT_Report (API, GMT_MSG_NORMAL, "Selection led to no output columns.\n");
		Return (GMT_RUNTIME_ERROR);

	}
	if ((error = gmt_set_cols (GMT, GMT_OUT, n_cols_out)) != GMT_NOERROR) {
		Return (error);
	}

	if (Ctrl->S.active && GMT->current.io.ogr == GMT_OGR_TRUE && (p = strchr (Ctrl->S.select->pattern[0], '=')) != NULL) {	/* Want to search for an aspatial value */
		*p = 0;	/* Skip the = sign */
		if ((Ctrl->S.select->ogr_item = gmt_get_ogr_id (GMT->current.io.OGR, Ctrl->S.select->pattern[0])) != GMT_NOTSET) {
			Ctrl->S.select->ogr_match = true;
			p++;
			strcpy (Ctrl->S.select->pattern[0], p);	/* Move the value over to the start */
		}
	}

	/* We now know the exact number of segments and columns and an upper limit on total records.
	 * Allocate data set with a single table with those proportions. This copies headers as well */

	D[GMT_IN]->dim[GMT_COL] = n_cols_out;	/* State we want a different set of columns on output */
	D[GMT_OUT] = GMT_Duplicate_Data (API, GMT_IS_DATASET, GMT_DUPLICATE_ALLOC + ((Ctrl->A.active) ? GMT_ALLOC_HORIZONTAL : GMT_ALLOC_NORMAL), D[GMT_IN]);

	n_horizontal_tbls = (Ctrl->A.active) ? D[GMT_IN]->n_tables : 1;	/* Only with pasting do we go horizontally */
	n_vertical_tbls   = (Ctrl->A.active) ? 1 : D[GMT_IN]->n_tables;	/* Only for concatenation do we go vertically */
	val = gmt_M_memory (GMT, NULL, n_cols_in, double);

	for (tbl_ver = 0; tbl_ver < n_vertical_tbls; tbl_ver++) {	/* Number of tables to place vertically */
		D[GMT_OUT]->table[tbl_ver]->n_records = 0;	/* Reset record count per table since we may return fewer than the original */
		for (seg = 0; seg < D[GMT_IN]->table[tbl_ver]->n_segments; seg++) {	/* For each segment in the tables */
			if (Ctrl->L.active) D[GMT_OUT]->table[tbl_ver]->segment[seg]->mode = GMT_WRITE_HEADER;	/* Only write segment header */
			if (Ctrl->S.active) {		/* See if the combined segment header has text matching our search string */
				match = gmt_get_text_selection (GMT, Ctrl->S.select, D[GMT_IN]->table[tbl_ver]->segment[seg], match);
				if (Ctrl->S.select->invert == match) D[GMT_OUT]->table[tbl_ver]->segment[seg]->mode = GMT_WRITE_SKIP;	/* Mark segment to be skipped */
			}
			if (Ctrl->Q.active && !gmt_get_int_selection (GMT, Ctrl->Q.select, seg)) D[GMT_OUT]->table[tbl_ver]->segment[seg]->mode = GMT_WRITE_SKIP;	/* Mark segment to be skipped */
			if (Ctrl->C.active) {	/* See if the number of records in this segment passes our test for output */
				match = (D[GMT_IN]->table[tbl_ver]->segment[seg]->n_rows >= Ctrl->C.min && D[GMT_IN]->table[tbl_ver]->segment[seg]->n_rows <= Ctrl->C.max);
				if (Ctrl->C.invert == match) D[GMT_OUT]->table[tbl_ver]->segment[seg]->mode = GMT_WRITE_SKIP;	/* Mark segment to be skipped */
			}
			if (D[GMT_OUT]->table[tbl_ver]->segment[seg]->mode) continue;	/* No point copying values given segment content will be skipped */
			n_out_seg++;	/* Number of segments that passed the test */
			last_row = D[GMT_IN]->table[tbl_ver]->segment[seg]->n_rows - 1;
			for (row = n_rows = 0; row <= last_row; row++) {	/* Go down all the rows */
				if (!Ctrl->E.active) { /* Skip this section */ }
				else if (Ctrl->E.mode < 0) {	/* Only pass first or last or both of them, skipping all others */
					if (row > 0 && row < last_row) continue;		/* Always skip the middle of the segment */
					if (row == 0 && !(-Ctrl->E.mode & 1)) continue;		/* First record, but we are to skip it */
					if (row == last_row && !(-Ctrl->E.mode & 2)) continue;	/* Last record, but we are to skip it */
				}
				else {	/* Only pass modulo E.mode records */
					if (row % Ctrl->E.mode != 0) continue;
				}
				/* Pull out current virtual row (may consist of a single or many (-A) table rows) */
				for (tbl_hor = out_col = 0; tbl_hor < n_horizontal_tbls; tbl_hor++) {	/* Number of tables to place horizontally */
					use_tbl = (Ctrl->A.active) ? tbl_hor : tbl_ver;
					for (col = 0; col < D[GMT_IN]->table[use_tbl]->segment[seg]->n_columns; col++, out_col++) {	/* Now go across all columns in current table */
						val[out_col] = D[GMT_IN]->table[use_tbl]->segment[seg]->data[col][row];
					}
				}
				for (col = 0; col < n_cols_out; col++) {	/* Now go across the single virtual row */
					if (col >= n_cols_in) continue;			/* This column is beyond end of this table */
					D[GMT_OUT]->table[tbl_ver]->segment[seg]->data[col][n_rows] = val[col];
				}
				n_rows++;
			}
			D[GMT_OUT]->table[tbl_ver]->segment[seg]->id = out_seg++;
			D[GMT_OUT]->table[tbl_ver]->segment[seg]->n_rows = n_rows;	/* Possibly shorter than originally allocated if -E is used */
			D[GMT_OUT]->table[tbl_ver]->n_records += n_rows;
			D[GMT_OUT]->n_records = D[GMT_OUT]->table[tbl_ver]->n_records;
			if (D[GMT_IN]->table[tbl_ver]->segment[seg]->ogr) gmt_duplicate_ogr_seg (GMT, D[GMT_OUT]->table[tbl_ver]->segment[seg], D[GMT_IN]->table[tbl_ver]->segment[seg]);
		}
		D[GMT_OUT]->table[tbl_ver]->id = tbl_ver;
	}
	gmt_M_free (GMT, val);

	if (Ctrl->I.active) {	/* Must reverse the order of tables, segments and/or records */
		uint64_t tbl1, tbl2, row1, row2, seg1, seg2;
		void *p = NULL;
		if (Ctrl->I.mode & INV_ROWS) {	/* Must actually swap rows */
			GMT_Report (API, GMT_MSG_LONG_VERBOSE, "Reversing order of records within each segment.\n");
			for (tbl = 0; tbl < D[GMT_OUT]->n_tables; tbl++) {	/* Number of output tables */
				for (seg = 0; seg < D[GMT_OUT]->table[tbl]->n_segments; seg++) {	/* For each segment in the tables */
					for (row1 = 0, row2 = D[GMT_OUT]->table[tbl]->segment[seg]->n_rows - 1; row1 < D[GMT_OUT]->table[tbl]->segment[seg]->n_rows/2; row1++, row2--) {
						for (col = 0; col < D[GMT_OUT]->table[tbl]->segment[seg]->n_columns; col++)
							gmt_M_double_swap (D[GMT_OUT]->table[tbl]->segment[seg]->data[col][row1], D[GMT_OUT]->table[tbl]->segment[seg]->data[col][row2]);
					}
				}
			}
		}
		if (Ctrl->I.mode & INV_SEGS) {	/* Must reorder pointers to segments within each table */
			GMT_Report (API, GMT_MSG_LONG_VERBOSE, "Reversing order of segments within each table.\n");
			for (tbl = 0; tbl < D[GMT_OUT]->n_tables; tbl++) {	/* Number of output tables */
				for (seg1 = 0, seg2 = D[GMT_OUT]->table[tbl]->n_segments-1; seg1 < D[GMT_OUT]->table[tbl]->n_segments/2; seg1++, seg2--) {	/* For each segment in the table */
					p = D[GMT_OUT]->table[tbl]->segment[seg1];
					D[GMT_OUT]->table[tbl]->segment[seg1] = D[GMT_OUT]->table[tbl]->segment[seg2];
					D[GMT_OUT]->table[tbl]->segment[seg2] = p;
				}
			}
		}
		if (Ctrl->I.mode & INV_TBLS) {	/* Must reorder pointers to tables within dataset  */
			GMT_Report (API, GMT_MSG_LONG_VERBOSE, "Reversing order of tables within the data set.\n");
			for (tbl1 = 0, tbl2 = D[GMT_OUT]->n_tables-1; tbl1 < D[GMT_OUT]->n_tables/2; tbl1++, tbl2--) {	/* For each table */
				p = D[GMT_OUT]->table[tbl1];
				D[GMT_OUT]->table[tbl1] = D[GMT_OUT]->table[tbl2];
				D[GMT_OUT]->table[tbl2] = p;
			}
		}
	}

	/* Now ready for output */

	if (Ctrl->D.active) {	/* Set composite name and io-mode */
		gmt_M_str_free (Ctrl->Out.file);
		Ctrl->Out.file = strdup (Ctrl->D.name);
		D[GMT_OUT]->io_mode = Ctrl->D.mode;
		if (Ctrl->D.origin) {	/* Update IDs */
			for (tbl = 0; tbl < D[GMT_OUT]->n_tables; tbl++) {	/* For each table */
				D[GMT_OUT]->table[tbl]->id += Ctrl->D.t_orig;
				for (seg = 0; seg < D[GMT_OUT]->table[tbl]->n_segments; seg++)	/* For each segment */
					D[GMT_OUT]->table[tbl]->segment[seg]->id += Ctrl->D.s_orig;
			}
		}
	}
	else {	/* Just register output to stdout or the given file via ->outfile */
		if (GMT->common.a.output)	/* Must notify the machinery of this output type */
			D[GMT_OUT]->io_mode = GMT_WRITE_OGR;
	}

	if (Ctrl->T.active && gmt_M_file_is_memory (Ctrl->Out.file) && D[GMT_OUT]->n_segments > 1) {
		/* Since no file is written we must physically collate segments into a single segment per table first */
		unsigned int flag[3] = {0, 0, GMT_WRITE_SEGMENT};
		struct GMT_DATASET *D2 = NULL;
		if ((D2 = GMT_Convert_Data (API, D[GMT_OUT], GMT_IS_DATASET, NULL, GMT_IS_DATASET, flag)) == NULL) {
			GMT_Report (API, GMT_MSG_LONG_VERBOSE, "Error collating each table's segments into a single segment per table.\n");
			Return (API->error);
		}
		if (GMT_Destroy_Data (API, &D[GMT_OUT]) != GMT_NOERROR) {	/* Remove the previously registered output dataset */
			Return (API->error);
		}
		D[GMT_OUT] = D2;	/* Hook up the reformatted dataset */
	}

	if (GMT_Write_Data (API, GMT_IS_DATASET, GMT_IS_FILE, D[GMT_IN]->geometry, D[GMT_OUT]->io_mode, NULL, Ctrl->Out.file, D[GMT_OUT]) != GMT_NOERROR) {
		Return (API->error);
	}
	if (prevent_seg_headers) GMT->current.io.skip_headers_on_outout = false;	/* Restore to default if it was changed for file output */

	GMT_Report (API, GMT_MSG_VERBOSE, "%" PRIu64 " tables %s, %" PRIu64 " records passed (input cols = %d; output cols = %d)\n",
		D[GMT_IN]->n_tables, method[Ctrl->A.active], D[GMT_OUT]->n_records, n_cols_in, n_cols_out);
	if (Ctrl->Q.active || Ctrl->S.active) GMT_Report (API, GMT_MSG_VERBOSE, "Extracted %" PRIu64 " from a total of %" PRIu64 " segments\n", n_out_seg, D[GMT_OUT]->n_segments);

	Return (GMT_NOERROR);
}
