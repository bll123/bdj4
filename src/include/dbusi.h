#ifndef INC_DBUSI_H
#define INC_DBUSI_H

#include <stdbool.h>

typedef struct dbus dbus_t;

dbus_t * dbusConnInit (void);
void dbusConnClose (dbus_t *dbus);
void dbusMessageInit (dbus_t *dbus);
void dbusMessageSetData (dbus_t *dbus, const char *sdata, ...);
void dbusMessageSetDataString (dbus_t *dbus, const char *sdata, ...);
bool dbusMessage (dbus_t *dbus, const char *bus, const char *objpath, const char *intfc, const char *method);
bool dbusResultGet (dbus_t *dbus, ...);

#endif /* INC_DBUSI_H */