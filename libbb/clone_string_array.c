/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

/*
 * Create a new heap allocated clone of a NULL
 * terminated string array.
 */
char FAST_FUNC **clone_string_array(char *const *original)
{
	char **copied_array, *const *temporary_pointer;

	/* find the end of original array */
	for (temporary_pointer = original; *temporary_pointer; ++temporary_pointer);

	/* allocate a new char** */
	copied_array = xmalloc(sizeof(char *) * (temporary_pointer - original + 1));

	/* duplicate all strings */
	for (temporary_pointer = original; *temporary_pointer; ++temporary_pointer)
		copied_array[temporary_pointer - original] = xstrdup(*temporary_pointer);

	/* make sure the copied array is terminated */
	copied_array[temporary_pointer - original] = NULL;

	return copied_array;
}