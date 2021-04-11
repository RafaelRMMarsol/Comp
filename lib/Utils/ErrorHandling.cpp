//===-- ErrorHandling.cpp -------------------------------------------------===//
//
//                     The inception Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Utils/ErrorHandling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>

#include <set>

using namespace inception;
using namespace llvm;

FILE *inception::inception_warning_file = NULL;
FILE *inception::inception_message_file = NULL;

static const char *warningPrefix = "WARNING";
static const char *warningOncePrefix = "WARNING ONCE";
static const char *errorPrefix = "ERROR";
static const char *notePrefix = "NOTE";

namespace {
cl::opt<bool> WarningsOnlyToFile(
    "warnings-only-to-file", cl::init(false),
    cl::desc("All warnings will be written to warnings.txt only.  If disabled, "
             "they are also written on screen."));
}

static bool shouldSetColor(const char *pfx, const char *msg,
                           const char *prefixToSearchFor) {
  if (pfx && strcmp(pfx, prefixToSearchFor) == 0)
    return true;

  if (llvm::StringRef(msg).startswith(prefixToSearchFor))
    return true;

  return false;
}

static void inception_vfmessage(FILE *fp, const char *pfx, const char *msg,
                           va_list ap) {
  if (!fp)
    return;

  llvm::raw_fd_ostream fdos(fileno(fp), /*shouldClose=*/false,
                            /*unbuffered=*/true);
  bool modifyConsoleColor = fdos.is_displayed() && (fp == stderr);
  modifyConsoleColor = true;

  if (modifyConsoleColor) {

    // Warnings
    if (shouldSetColor(pfx, msg, warningPrefix))
      fdos.changeColor(llvm::raw_ostream::MAGENTA,
                       /*bold=*/false,
                       /*bg=*/false);

    // Once warning
    if (shouldSetColor(pfx, msg, warningOncePrefix))
      fdos.changeColor(llvm::raw_ostream::MAGENTA,
                       /*bold=*/true,
                       /*bg=*/false);

    // Errors
    if (shouldSetColor(pfx, msg, errorPrefix))
      fdos.changeColor(llvm::raw_ostream::RED,
                       /*bold=*/true,
                       /*bg=*/false);

    // Notes
    if (shouldSetColor(pfx, msg, notePrefix))
      fdos.changeColor(llvm::raw_ostream::BLUE,
                       /*bold=*/true,
                       /*bg=*/false);
  }

  fdos << "\t";
  if (pfx)
    fdos << pfx << ": ";

  // FIXME: Can't use fdos here because we need to print
  // a variable number of arguments and do substitution
  vfprintf(fp, msg, ap);
  fflush(fp);

  fdos << "\n";

  if (modifyConsoleColor)
    fdos.resetColor();

  fdos.flush();
}

/* Prints a message/warning.

   If pfx is NULL, this is a regular message, and it's sent to
   inception_message_file (messages.txt).  Otherwise, it is sent to
   inception_warning_file (warnings.txt).

   Iff onlyToFile is false, the message is also printed on stderr.
*/
static void inception_vmessage(const char *pfx, bool onlyToFile, const char *msg,
                          va_list ap) {
  if (!onlyToFile) {
    va_list ap2;
    va_copy(ap2, ap);
    inception_vfmessage(stderr, pfx, msg, ap2);
    va_end(ap2);
  }

  inception_vfmessage(pfx ? inception_warning_file : inception_message_file, pfx, msg, ap);
}

void inception::inception_message(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  inception_vmessage(NULL, false, msg, ap);
  va_end(ap);
}

/* Message to be written only to file */
void inception::inception_message_to_file(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  inception_vmessage(NULL, true, msg, ap);
  va_end(ap);
}

void inception::inception_error(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  inception_vmessage(errorPrefix, false, msg, ap);
  va_end(ap);
  exit(1);
}

void inception::inception_warning(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  inception_vmessage(warningPrefix, WarningsOnlyToFile, msg, ap);
  va_end(ap);
}

/* Prints a warning once per message. */
void inception::inception_warning_once(const void *id, const char *msg, ...) {
  static std::set<std::pair<const void *, const char *> > keys;
  std::pair<const void *, const char *> key;

  /* "calling external" messages contain the actual arguments with
     which we called the external function, so we need to ignore them
     when computing the key. */
  if (strncmp(msg, "calling external", strlen("calling external")) != 0)
    key = std::make_pair(id, msg);
  else
    key = std::make_pair(id, "calling external");

  if (!keys.count(key)) {
    keys.insert(key);
    va_list ap;
    va_start(ap, msg);
    inception_vmessage(warningOncePrefix, WarningsOnlyToFile, msg, ap);
    va_end(ap);
  }
}