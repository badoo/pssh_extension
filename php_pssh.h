/*
  +----------------------------------------------------------------------+
  | PHP Version 4                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2006 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Antony Dovgal <tony@daylessday.org>                          |
  +----------------------------------------------------------------------+
*/

/* $Id: php_pssh.h,v 1.2 2007/12/24 17:44:26 tony Exp $ */

#ifndef PHP_PSSH_H
#define PHP_PSSH_H

#define PHP_PSSH_VERSION "1.0.0"

extern zend_module_entry pssh_module_entry;
#define phpext_pssh_ptr &pssh_module_entry

#ifdef PHP_WIN32
#define PHP_PSSH_API __declspec(dllexport)
#else
#define PHP_PSSH_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#endif	/* PHP_PSSH_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
