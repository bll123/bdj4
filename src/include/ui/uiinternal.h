#ifndef INC_UIINTERNAL_H
#define INC_UIINTERNAL_H

# if BDJ4_USE_GTK /* gtk3 */

#  include <gtk/gtk.h>

typedef struct uiwidget {
  union {
    GtkWidget         *widget;
    GtkSizeGroup      *sg;
    GdkPixbuf         *pixbuf;
    GtkTreeSelection  *sel;
    GtkTextBuffer     *buffer;
    GtkAdjustment     *adjustment;
  };
} uiwcont_t;

# endif /* BDJ4_USE_GTK */

#endif /* INC_UIINTERNAL_H */
