#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "json.h"

int main(int argc, char **argv)
{
	MC_SET_DEBUG(1);

	/*
	 * Check that replacing an existing object keeps the key valid,
	 * and that it keeps the order the same.
	 */
	json_object *old_object = json_object_new_object();
	json_object_object_add(old_object, "foo2", json_object_new_string("bar1"));

	json_object *my_object = json_object_new_object();

	json_object_object_add(my_object, "foo2", json_object_new_string("bar2"));
	json_object *tmp;
	if (json_object_object_get_ex(my_object, "foo2", &tmp))
	  printf("%s\n", json_object_get_string(tmp));
	
	if (json_object_object_get_ex(old_object, "foo2", &tmp))
	  json_object_object_add(my_object, "foo2", tmp);

	if (json_object_object_get_ex(my_object, "foo2", &tmp))
	  printf("%s\n", json_object_get_string(tmp));

	return 0;
}
