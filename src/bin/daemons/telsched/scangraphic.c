/* code to manage the stuff on the scan graphic display.
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/DrawingA.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/ScrolledW.h>

#include "P_.h"
#include "astro.h"
#include "circum.h"
#include "xtools.h"
#include "configfile.h"
#include "scan.h"

#include "telsched.h"

#define	SG_W	130	/* table width */
#define	SG_H	800	/* table height */

static void sg_create_form (void);
static void sg_da_exp_cb (Widget w, XtPointer client, XtPointer call);
static void sg_close_cb (Widget w, XtPointer client, XtPointer call);
static void sg_all (void);
static void sg_stats (Obs *op, int nop);
static void make_gcs (Display *dsp, Window win);

static Widget sgform_w;	/* overall form dialog */
static Widget sgda_w;	/* drawing area */
static Widget stats_w;	/* label to hold the stats */

static char clsn_c[] = "#ee4040";	/* color for collisions */
static char offn_c[] = "#808080";	/* color for items marks off */
static GC c_gc, o_gc,w_gc,fg_gc,bg_gc;	/* GCs */

void
sg_manage ()
{
	if (!sgform_w)
	    sg_create_form();
	
	if (XtIsManaged(sgform_w))
	    XtUnmanageChild (sgform_w);
	else
	    XtManageChild (sgform_w);
}

void
sg_update ()
{
	if (!sgform_w || !XtIsManaged(sgform_w))
	    return;

	sg_all ();
}

double
lst2utc (l)
double l;
{
	Now *np = &now;
	double today = mjd_day (np->n_mjd);
	double lngcor = radhr (np->n_lng);
	double u;

	l -= lngcor;
	range (&l, 24.0);
	gst_utc (today, l, &u);
	return (u);
}

static void
sg_create_form()
{
	Widget c_w, sw_w;
	Arg args[20];
	int n;

	/* create master form */
	n = 0;
	XtSetArg (args[n], XmNresizePolicy, XmRESIZE_GROW); n++;
	XtSetArg (args[n], XmNhorizontalSpacing, 5); n++;
	XtSetArg (args[n], XmNverticalSpacing, 5); n++;
	sgform_w = XmCreateFormDialog (toplevel_w, "ScanGF", args, n);

	/* set some stuff in the parent DialogShell.
	 * setting XmNdialogTitle in the Form didn't work..
	 */
	n = 0;
	XtSetArg (args[n], XmNtitle, "Scan Timetable"); n++;
	XtSetValues (XtParent(sgform_w), args, n);

	/* put a big label on top for the stats */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	stats_w = XmCreateLabel (sgform_w, "SL", args, n);
	XtManageChild (stats_w);

	/* make a Close botton at the bottom */
	n = 0;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNleftPosition, 30); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_POSITION); n++;
	XtSetArg (args[n], XmNrightPosition, 70); n++;
	XtSetArg (args[n], XmNalignment, XmALIGNMENT_CENTER); n++;
	c_w = XmCreatePushButton (sgform_w, "Close", args, n);
	XtManageChild (c_w);
	XtAddCallback (c_w, XmNactivateCallback, sg_close_cb, NULL);

	/* make a drawing area for the table in a scrolled window */

	n = 0;
	XtSetArg (args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNtopWidget, stats_w); n++;
	XtSetArg (args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg (args[n], XmNbottomWidget, c_w); n++;
	XtSetArg (args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg (args[n], XmNscrollingPolicy, XmAUTOMATIC); n++;
	XtSetArg (args[n], XmNheight, SG_H/2); n++;
	sw_w = XmCreateScrolledWindow (sgform_w, "SW", args, n);
	XtManageChild (sw_w);

	    n = 0;
	    XtSetArg (args[n], XmNwidth, SG_W); n++;
	    XtSetArg (args[n], XmNheight, SG_H); n++;
	    XtSetArg (args[n], XmNresizePolicy, XmRESIZE_NONE); n++;
	    sgda_w = XmCreateDrawingArea (sw_w, "SGDA", args, n);
	    XtManageChild (sgda_w);
	    XtAddCallback (sgda_w, XmNexposeCallback, sg_da_exp_cb, NULL);

	    n = 0;
	    XtSetArg (args[n], XmNworkWindow, sgda_w); n++;
	    XtSetValues (sw_w, args, n);
}


/* close */
/* ARGSUSED */
static void
sg_close_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XtUnmanageChild (sgform_w);
}

/* expose (or reconfig) of drawing area.
 * just redraw the scene to the current window size.
 */
/* ARGSUSED */
static void
sg_da_exp_cb (w, client, call)
Widget w;
XtPointer client;
XtPointer call;
{
	XmDrawingAreaCallbackStruct *c = (XmDrawingAreaCallbackStruct *)call;

	switch (c->reason) {
	case XmCR_EXPOSE: {
	    /* turn off gravity so we get expose events for either shrink or
	     * expand.
	     */
	    static int before;
	    XExposeEvent *e = &c->event->xexpose;

	    if (!before) {
		XSetWindowAttributes swa;
		swa.bit_gravity = ForgetGravity;
		XChangeWindowAttributes (e->display, e->window,
							    CWBitGravity, &swa);
		before = 1;
	    }
	    /* wait for the last in the series */
	    if (e->count != 0)
		return;
	    break;
	    }
	default:
	    printf ("Unexpected gda_w event. type=%d\n", c->reason);
	    exit(1);
	}

	sg_all ();
}

/* draw everything from scratch.
 */
static void
sg_all ()
{
#define	WINY(utc)	((int)(((utc)+2)*h/25))
				/* window y coord given utc in hrs.
				 * -1 can be used for column headings.
				 */
#define	SCANLINEWIDTH	5	/* line width of time lines */
#define	NSEGCACHE	100	/* draw these many line segments at once */
	Display *dsp = XtDisplay (sgda_w);
	XSegment xsegs[NSEGCACHE];
	int nxsegs;
	Window win = XtWindow (sgda_w);
	Window root;
	int x, y;
	unsigned int bw, d;
	unsigned w, h;		/* actual size of drawing area window */
	Pixmap pm;
	Obs *op;
	Obs *laston;
	int nop;
	int i;

	if (!fg_gc)
	    make_gcs(dsp, win);

	/* get size of window now.
	 * make a pixmap to match and use it then copy -- looks better.
	 */
	XGetGeometry (dsp, win, &root, &x, &y, &w, &h, &bw, &d);
	pm = XCreatePixmap (dsp, win, w, h, d);
	XFillRectangle (dsp, pm, bg_gc, 0, 0, w, h);

	/* set utc and lst calibrations */
	XDrawString (dsp, pm, fg_gc, 5, WINY(-1), "UTC", 3);
	XDrawString (dsp, pm, fg_gc, w - 20, WINY(-1), "LST", 3);
	for (i = 0; i < 24; i++) {
	    char buf[16];
	    double u;

	    sprintf (buf, "%2d", i);

	    /* draw the utc string */
	    XDrawImageString (dsp, pm, at_night (i*1.0) ? fg_gc : bg_gc,
					5, WINY(i), buf, strlen(buf));
	    XDrawLine (dsp, pm, fg_gc, 25, WINY(i), 30, WINY(i));

	    /* draw the lst string */
	    u = lst2utc ((double)i);
	    XDrawImageString (dsp, pm, at_night (u*1.0) ? fg_gc : bg_gc,
					w - 20, WINY(u), buf, strlen(buf));
	    XDrawLine (dsp, pm, fg_gc, w - 30, WINY(u), w - 25, WINY(u));
	}

	/* draw obs array in the pixmap */
	get_obsarray (&op, &nop);

	/* compute and display the statistics */
	sg_stats(op, nop);

	/* draw the timeline */
	nxsegs = 0;
	for (i = 0; i < nop; i++) {
	    Obs *tmpop = op+i;
	    XSegment *xsp;

	    /* TODO: why are some utcstart left 0? */
	    if (tmpop->off || tmpop->utcstart == 0 || tmpop->utcstart == NOTIME)
		continue;

	    xsp = &xsegs[nxsegs++];
	    xsp->x1 = w/2;
	    xsp->y1 = WINY(tmpop->utcstart);
	    xsp->x2 = w/2;
	    xsp->y2 = WINY(tmpop->utcstart + tmpop->scan.dur/3600)+1;
	    if (nxsegs == NSEGCACHE) {
		XDrawSegments (dsp, pm, w_gc, xsegs, nxsegs);
		nxsegs = 0;
	    }
	}
	if (nxsegs > 0)
	    XDrawSegments (dsp, pm, w_gc, xsegs, nxsegs);

	/* add with collisions */
	nxsegs = 0;
	laston = NULL;
	for (i = 0; i < nop; i++) {
	    Obs *tmpop = op+i;

	    /* TODO: why are some utcstart left 0? */
	    if (tmpop->off || tmpop->utcstart == 0 || tmpop->utcstart == NOTIME)
		continue;

	    if (laston && elapsedt (laston->utcstart + laston->scan.dur/3600,
							tmpop->utcstart) < 0) {
		XSegment *xsp = &xsegs[nxsegs++];
		xsp->x1 = 2*w/3;
		xsp->y1 = WINY(laston->utcstart + laston->scan.dur/3600);
		xsp->x2 = 2*w/3;
		xsp->y2 = WINY(tmpop->utcstart) + 1;
		if (nxsegs >= NSEGCACHE-2) {
		    XDrawSegments (dsp, pm, c_gc, xsegs, nxsegs);
		    nxsegs = 0;
		}
	    }

	    laston = tmpop;
	}
	if (nxsegs > 0)
	    XDrawSegments (dsp, pm, c_gc, xsegs, nxsegs);

	/* add the ones off */
	nxsegs = 0;
	laston = NULL;
	for (i = 0; i < nop; i++) {
	    Obs *tmpop = op+i;

	    if (tmpop->off) {
		XSegment *xsp = &xsegs[nxsegs++];
		xsp->x1 = 1*w/3;
		xsp->y1 = WINY(tmpop->utcstart);
		xsp->x2 = 1*w/3;
		xsp->y2 = WINY(tmpop->utcstart + tmpop->scan.dur/3600) + 1;
		if (nxsegs >= NSEGCACHE-2) {
		    XDrawSegments (dsp, pm, o_gc, xsegs, nxsegs);
		    nxsegs = 0;
		}
	    }

	    laston = tmpop;
	}
	if (nxsegs > 0)
	    XDrawSegments (dsp, pm, o_gc, xsegs, nxsegs);

	/* copy to the window and free the pixmap */
	XCopyArea (dsp, pm, win, fg_gc, 0, 0, w, h, 0, 0);
	XFreePixmap (dsp, pm);
}

/* compute and put the stats in stats_w
 */
static void
sg_stats(op, nop)
Obs *op;
int nop;
{
	int nlststart = 0;	/* N objects with LSTSTART set */
	int ncirpol = 0;	/* N circumpolar objects */
	int nneverup = 0;	/* N object never up */
	int noff = 0;		/* N off */
	int i;

	for (i = 0; i < nop; i++) {
	    Obs *tmpp = op+i;

	    if (tmpp->lststart != NOTIME)
		nlststart++;
	    if (tmpp->rs.rs_flags & (RS_CIRCUMPOLAR))
		ncirpol++;
	    if (tmpp->rs.rs_flags & (RS_NEVERUP|RS_ERROR)) {
		nneverup++;
		continue;
	    }
	    if (tmpp->off) {
		noff++;
		continue;
	    }
	}

	wlprintf (stats_w,
			   "%4d Scans      %4d Off\n"
			   "%4d NvrUp      %4d CirPol\n"
			   "%4d LSTSTART\n",
				    nop, noff,
				    nneverup, ncirpol,
				    nlststart);
}

/* make gcs from the DrawingArea colors
 */
static void
make_gcs (dsp, win)
Display *dsp;
Window win;
{
	XGCValues gcv;
	unsigned int gcm;
	Colormap def_cm = DefaultColormap (dsp, 0);
	XColor defxc, dbxc;
	Pixel red, off, fg, bg;

	get_something (sgda_w, XmNforeground, (char *)&fg);
	get_something (sgda_w, XmNbackground, (char *)&bg);

	if (!XAllocNamedColor (dsp, def_cm, clsn_c, &defxc, &dbxc))
	    red = BlackPixel (dsp, 0);
	else
	    red = defxc.pixel;
	if (!XAllocNamedColor (dsp, def_cm, offn_c, &defxc, &dbxc))
	    off = BlackPixel (dsp, 0);
	else
	    off = defxc.pixel;

	gcm = GCForeground | GCBackground | GCLineWidth;

	gcv.foreground = fg;
	gcv.background = bg;
	gcv.line_width = SCANLINEWIDTH;
	w_gc = XCreateGC (dsp, win, gcm, &gcv);

	gcv.foreground = red;
	gcv.background = bg;
	gcv.line_width = SCANLINEWIDTH;
	c_gc = XCreateGC (dsp, win, gcm, &gcv);

	gcv.foreground = off;
	gcv.background = bg;
	gcv.line_width = SCANLINEWIDTH;
	o_gc = XCreateGC (dsp, win, gcm, &gcv);

	gcv.foreground = fg;
	gcv.background = bg;
	gcv.line_width = 0;
	fg_gc = XCreateGC (dsp, win, gcm, &gcv);

	gcv.foreground = bg;
	gcv.background = fg;
	gcv.line_width = 0;
	bg_gc = XCreateGC (dsp, win, gcm, &gcv);
}

