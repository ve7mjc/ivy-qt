#ifndef IVYQT_GLOBAL_H
#define IVYQT_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(IVYQT_LIBRARY)
#  define IVYQTSHARED_EXPORT Q_DECL_EXPORT
#else
#  define IVYQTSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // IVYQT_GLOBAL_H
