/*
 * parameter.c
 *
 * Contains miscellaneous utilities used by PyCSH.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#include "utils.h"

#include <param/param_server.h>
#include <param/param_client.h>

#include "lib/param/src/param/param_string.h"

#include "pycsh.h"
#include "parameter/parameter.h"
#include "parameter/parameterarray.h"
#include "parameter/parameterlist.h"

/* Source: https://pythonextensionpatterns.readthedocs.io/en/latest/super_call.html */
PyObject * call_super_pyname_lookup(PyObject *self, PyObject *func_name, PyObject *args, PyObject *kwargs) {
    PyObject *result        = NULL;
    PyObject *builtins      = NULL;
    PyObject *super_type    = NULL;
    PyObject *super         = NULL;
    PyObject *super_args    = NULL;
    PyObject *func          = NULL;

    builtins = PyImport_AddModule("builtins");
    if (! builtins) {
        assert(PyErr_Occurred());
        goto except;
    }
    // Borrowed reference
    Py_INCREF(builtins);
    super_type = PyObject_GetAttrString(builtins, "super");
    if (! super_type) {
        assert(PyErr_Occurred());
        goto except;
    }
    super_args = PyTuple_New(2);
    Py_INCREF(self->ob_type);
    if (PyTuple_SetItem(super_args, 0, (PyObject*)self->ob_type)) {
        assert(PyErr_Occurred());
        goto except;
    }
    Py_INCREF(self);
    if (PyTuple_SetItem(super_args, 1, self)) {
        assert(PyErr_Occurred());
        goto except;
    }
    super = PyObject_Call(super_type, super_args, NULL);
    if (! super) {
        assert(PyErr_Occurred());
        goto except;
    }
    func = PyObject_GetAttr(super, func_name);
    if (! func) {
        assert(PyErr_Occurred());
        goto except;
    }
    if (! PyCallable_Check(func)) {
        PyErr_Format(PyExc_AttributeError,
                     "super() attribute \"%S\" is not callable.", func_name);
        goto except;
    }
    result = PyObject_Call(func, args, kwargs);
    assert(! PyErr_Occurred());
    goto finally;
except:
    assert(PyErr_Occurred());
    Py_XDECREF(result);
    result = NULL;
finally:
    Py_XDECREF(builtins);
    Py_XDECREF(super_args);
    Py_XDECREF(super_type);
    Py_XDECREF(super);
    Py_XDECREF(func);
    return result;
}


/* Retrieves a param_t from either its name, id or wrapper object.
   May raise TypeError or ValueError, returned value will be NULL in either case. */
param_t * _pycsh_util_find_param_t(PyObject * param_identifier, int node) {

	param_t * param = NULL;

	if (PyUnicode_Check(param_identifier))  // is_string
		param = param_list_find_name(node, (char*)PyUnicode_AsUTF8(param_identifier));
	else if (PyLong_Check(param_identifier))  // is_int
		param = param_list_find_id(node, (int)PyLong_AsLong(param_identifier));
	else if (PyObject_TypeCheck(param_identifier, &ParameterType))
		param = ((ParameterObject *)param_identifier)->param;
	else {  // Invalid type passed.
		PyErr_SetString(PyExc_TypeError,
			"Parameter identifier must be either an integer or string of the parameter ID or name respectively.");
		return NULL;
	}

	if (param == NULL)  // Check if a parameter was found.
		PyErr_SetString(PyExc_ValueError, "Could not find a matching parameter.");

	return param;  // or NULL for ValueError.
}


/* Gets the best Python representation of the param_t's type, i.e 'int' for 'uint32'.
   Does not increment the reference count of the found type before returning.
   May raise TypeError for unsupported parameter types (none exist at time of writing). */
static PyTypeObject * _pycsh_misc_param_t_type(param_t * param) {

	PyTypeObject * param_type = NULL;

	switch (param->type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
		case PARAM_TYPE_INT8:
		case PARAM_TYPE_INT16:
		case PARAM_TYPE_INT32:
		case PARAM_TYPE_INT64: {
			param_type = &PyLong_Type;
			break;
		}
		case PARAM_TYPE_FLOAT:
		case PARAM_TYPE_DOUBLE: {
			param_type = &PyFloat_Type;
			break;
		}
		case PARAM_TYPE_STRING: {
			param_type = &PyUnicode_Type;
			break;
		}
		case PARAM_TYPE_DATA: {
			param_type = &PyByteArray_Type;
			break;
		}
		default:  // Raise NotImplementedError when param_type remains NULL.
			PyErr_SetString(PyExc_NotImplementedError, "Unsupported parameter type.");
			break;
	}

	return param_type;  // or NULL (for NotImplementedError).
}


/* Public interface for '_pycsh_misc_param_t_type()'
   Increments the reference count of the found type before returning. */
PyObject * pycsh_util_get_type(PyObject * self, PyObject * args) {

	PyObject * param_identifier;
	int node = default_node;

	param_t * param;

	/* Function may be called either as method on 'Parameter' object or standalone function. */
	
	if (self && PyObject_TypeCheck(self, &ParameterType)) {
		ParameterObject *_self = (ParameterObject *)self;

		node = _self->param->node;
		param = _self->param;

	} else {
		if (!PyArg_ParseTuple(args, "O|i", &param_identifier, &node)) {
			return NULL;  // TypeError is thrown
		}

		param = _pycsh_util_find_param_t(param_identifier, node);
	}

	if (param == NULL) {  // Did not find a match.
		return NULL;  // Raises either TypeError or ValueError.
	}

	PyTypeObject * result = _pycsh_misc_param_t_type(param);
	Py_INCREF(result);

	return (PyObject *)result;
}


/* Create a Python Parameter object from a param_t pointer directly. */
PyObject * _pycsh_Parameter_from_param(PyTypeObject *type, param_t * param, int host, int timeout, int retries) {

	if (param->array_size <= 1 && type == &ParameterArrayType) {
		PyErr_SetString(PyExc_TypeError, 
			"Attempted to create an ParameterArray instance, for a non array parameter.");
		return NULL;
	} else if (param->array_size > 1)  // If the parameter is an array.
		type = &ParameterArrayType;  // We create a ParameterArray instance instead.
		// If you listen really carefully here, you can hear OOP idealists, screaming in agony.
		// On a more serious note, I'm amazed that this even works at all.

	ParameterObject *self = (ParameterObject *)type->tp_alloc(type, 0);

	if (self == NULL) {
		return NULL;
	}

	self->param = param;
	self->host = host;
	self->timeout = timeout;
	self->retries = retries;

	self->name = PyUnicode_FromString(param->name);
	if (self->name == NULL) {
		Py_DECREF(self);
		return NULL;
	}

	self->unit = (param->unit != NULL ? PyUnicode_FromString(param->unit) : Py_None);
	if (self->unit == NULL) {
		Py_DECREF(self);
		return NULL;
	}

	self->docstr = (param->docstr != NULL ? PyUnicode_FromString(param->docstr) : Py_None);
	if (self->docstr == NULL) {
		Py_DECREF(self);
		return NULL;
	}

	self->type = (PyTypeObject *)pycsh_util_get_type((PyObject *)self, NULL);

    return (PyObject *) self;
}


/* Constructs a list of Python Parameters of all known param_t returned by param_list_iterate. */
PyObject * pycsh_util_parameter_list() {

	PyObject * list = PyObject_CallObject((PyObject *)&ParameterListType, NULL);

	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {
		PyObject * parameter = _pycsh_Parameter_from_param(&ParameterType, param, INT_MIN, PYCSH_DFL_TIMEOUT, 1);
		PyObject * argtuple = PyTuple_Pack(1, parameter);
		Py_DECREF(ParameterList_append(list, argtuple));
		Py_DECREF(argtuple);
		Py_DECREF(parameter);
	}

	return list;

}

/* Checks that the specified index is within bounds of the sequence index, raises IndexError if not.
   Supports Python backwards subscriptions, mutates the index to a positive value in such cases. */
static int _pycsh_util_index(int seqlen, int *index) {
	if (*index < 0)  // Python backwards subscription.
		*index += seqlen;
	if (*index < 0 || *index > seqlen - 1) {
		PyErr_SetString(PyExc_IndexError, "Array Parameter index out of range");
		return -1;
	}

	return 0;
}

/* Private interface for getting the value of single parameter
   Increases the reference count of the returned item before returning.
   Use INT_MIN for offset as no offset. */
PyObject * _pycsh_util_get_single(param_t *param, int offset, int autopull, int host, int timeout, int retries) {

	if (offset != INT_MIN) {
		if (_pycsh_util_index(param->array_size, &offset))  // Validate the offset.
			return NULL;  // Raises IndexError.
	} else
		offset = -1;

	if (autopull && (param->node != 0)) {

		for (size_t i = 0; i < (retries > 0 ? retries : 1); i++)
			if (param_pull_single(param, offset, 1, (host != INT_MIN ? host : param->node), timeout, paramver))
				if (i >= retries-1) {
					PyErr_SetString(PyExc_ConnectionError, "No response");
					return NULL;
				}
	}

	param_print(param, -1, NULL, 0, 0);

	switch (param->type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
			if (offset != -1)
				return Py_BuildValue("B", param_get_uint8_array(param, offset));
			return Py_BuildValue("B", param_get_uint8(param));
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
			if (offset != -1)
				return Py_BuildValue("H", param_get_uint16_array(param, offset));
			return Py_BuildValue("H", param_get_uint16(param));
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
			if (offset != -1)
				return Py_BuildValue("I", param_get_uint32_array(param, offset));
			return Py_BuildValue("I", param_get_uint32(param));
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
			if (offset != -1)
				return Py_BuildValue("K", param_get_uint64_array(param, offset));
			return Py_BuildValue("K", param_get_uint64(param));
		case PARAM_TYPE_INT8:
			if (offset != -1)
				return Py_BuildValue("b", param_get_int8_array(param, offset));
			return Py_BuildValue("b", param_get_int8(param));
		case PARAM_TYPE_INT16:
			if (offset != -1)
				return Py_BuildValue("h", param_get_int16_array(param, offset));
			return Py_BuildValue("h", param_get_int16(param));
		case PARAM_TYPE_INT32:
			if (offset != -1)
				return Py_BuildValue("i", param_get_int32_array(param, offset));
			return Py_BuildValue("i", param_get_int32(param));
		case PARAM_TYPE_INT64:
			if (offset != -1)
				return Py_BuildValue("k", param_get_int64_array(param, offset));
			return Py_BuildValue("k", param_get_int64(param));
		case PARAM_TYPE_FLOAT:
			if (offset != -1)
				return Py_BuildValue("f", param_get_float_array(param, offset));
			return Py_BuildValue("f", param_get_float(param));
		case PARAM_TYPE_DOUBLE:
			if (offset != -1)
				return Py_BuildValue("d", param_get_double_array(param, offset));
			return Py_BuildValue("d", param_get_double(param));
		case PARAM_TYPE_STRING: {
			char buf[param->array_size];
			param_get_string(param, &buf, param->array_size);
			if (offset != -1) {
				char charstrbuf[] = {buf[offset]};
				return Py_BuildValue("s", charstrbuf);
			}
			return Py_BuildValue("s", buf);
		}
		case PARAM_TYPE_DATA: {
			// TODO Kevin: No idea if this has any chance of working.
			//	I hope it will raise a reasonable exception if it doesn't,
			//	instead of just segfaulting :P
			unsigned int size = (param->array_size > 1) ? param->array_size : 1;
			char buf[size];
			param_get_data(param, buf, size);
			return Py_BuildValue("O&", buf);
		}
	}
	PyErr_SetString(PyExc_NotImplementedError, "Unsupported parameter type for get operation.");
	return NULL;
}

/* Private interface for getting the value of an array parameter
   Increases the reference count of the returned tuple before returning.  */
PyObject * _pycsh_util_get_array(param_t *param, int autopull, int host, int timeout, int retries) {

	// Pull the value for every index using a queue (if we're allowed to),
	// instead of pulling them individually.
	if (autopull && param->node != 0) {
		void * queuebuffer = malloc(PARAM_SERVER_MTU);
		param_queue_t queue = { };
		param_queue_init(&queue, queuebuffer, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_GET, paramver);

		for (int i = 0; i < param->array_size; i++) {
			param_queue_add(&queue, param, i, NULL);
		}

		for (size_t i = 0; i < (retries > 0 ? retries : 1); i++) {
			if (param_pull_queue(&queue, 0, param->node, timeout)) {
				PyErr_SetString(PyExc_ConnectionError, "No response.");
				free(queuebuffer);
				return 0;
			}
		}

		free(queuebuffer);
	}
	
	// We will populate this tuple with the values from the indexes.
	PyObject * value_tuple = PyTuple_New(param->array_size);

	for (int i = 0; i < param->array_size; i++) {
		PyObject * item = _pycsh_util_get_single(param, i, 0, host, timeout, retries);

		if (item == NULL) {  // Something went wrong, probably a connectionerror. Let's abandon ship.
			Py_DECREF(value_tuple);
			return NULL;
		}
		
		PyTuple_SET_ITEM(value_tuple, i, item);
	}
	
	return value_tuple;
}



static PyObject * _pycsh_get_str_value(PyObject * obj) {

	// This 'if' exists for cases where the value 
	// of a parmeter is assigned from that of another.
	// i.e: 				param1.value = param2
	// Which is equal to:	param1.value = param2.value
	if (PyObject_TypeCheck(obj, &ParameterType)) {
		// Return the value of the Parameter.

		param_t * param = ((ParameterObject *)obj)->param;
		int host = ((ParameterObject *)obj)->host;
		int timeout = ((ParameterObject *)obj)->timeout;
		int retries = ((ParameterObject *)obj)->retries;

		PyObject * value = param->array_size > 0 ? 
			_pycsh_util_get_array(param, autosend, host, timeout, retries) :
			_pycsh_util_get_single(param, INT_MIN, autosend, host, timeout, retries);

		PyObject * strvalue = PyObject_Str(value);
		Py_DECREF(value);
		return strvalue;
	}
	else  // Otherwise use __str__.
		return PyObject_Str(obj);
}

/* Attempts a conversion to the specified type, by calling it. */
static PyObject * _pycsh_typeconvert(PyObject * strvalue, PyTypeObject * type, int check_only) {
	// TODO Kevin: Using this to check the types of object is likely against
	// PEP 20 -- The Zen of Python: "Explicit is better than implicit"

	PyObject * valuetuple = PyTuple_Pack(1, strvalue);
	PyObject * converted_value = PyObject_CallObject((PyObject *)type, valuetuple);
	if (converted_value == NULL) {
		Py_DECREF(valuetuple);  // converted_value is NULL, so we don't decrements its refcount.
		return NULL;  // We assume failed conversions to have set an exception string.
	}
	Py_DECREF(valuetuple);
	if (check_only) {
		Py_DECREF(converted_value);
		Py_RETURN_NONE;
	}
	return converted_value;
}

/* Iterates over the specified iterable, and checks the type of each object. */
static int _pycsh_typecheck_sequence(PyObject * sequence, PyTypeObject * type) {
	// This is likely not thread-safe however.

	// It seems that tuples pass PySequence_Check() but not PyIter_Check(),
	// which seems to indicate that not all sequences are inherently iterable.
	if (!PyIter_Check(sequence) && !PySequence_Check(sequence)) {
		PyErr_SetString(PyExc_TypeError, "Provided value is not a iterable");
		return -1;
	}

	PyObject *iter = PyObject_GetIter(sequence);

	PyObject *item;

	while ((item = PyIter_Next(iter)) != NULL) {

		if (!_pycsh_typeconvert(item, type, 1)) {
#if 0  // Should we raise the exception from the failed conversion, or our own?
			PyObject * temppystr = PyObject_Str(item);
			char* tempstr = (char*)PyUnicode_AsUTF8(temppystr);
			char buf[70 + strlen(item->ob_type->tp_name) + strlen(tempstr)];
			sprintf(buf, "Iterable contains object of an incorrect/unconvertible type <%s: %s>.", item->ob_type->tp_name, tempstr);
			PyErr_SetString(PyExc_TypeError, buf);
			Py_DECREF(temppystr);
#endif
			return -2;
		}
	}

	// Raise an exception if we got an error while iterating.
	return PyErr_Occurred() ? -3 : 0;
}

/* Private interface for setting the value of a normal parameter. 
   Use INT_MIN as no offset. */
int _pycsh_util_set_single(param_t *param, PyObject *value, int offset, int host, int timeout, int retries, param_queue_t *queue) {
	
	if (offset != INT_MIN) {
		if (param->type == PARAM_TYPE_STRING) {
			PyErr_SetString(PyExc_NotImplementedError, "Cannot set string parameters by index.");
			return -1;
		}

		if (_pycsh_util_index(param->array_size, &offset))  // Validate the offset.
			return -1;  // Raises IndexError.
	} else
		offset = -1;

	char valuebuf[128] __attribute__((aligned(16))) = { };
	PyObject * strvalue = _pycsh_get_str_value(value);
	param_str_to_value(param->type, (char*)PyUnicode_AsUTF8(strvalue), valuebuf);
	Py_DECREF(strvalue);

	// TODO Kevin: The structure of setting the parameters has been refactored,
	//	confirm that it still behaves like the original (especially for remote host parameters).
	if (queue == NULL) {  // Set the parameter immediately, if no queue is provided.

		if (param->node == 0) {
			if (offset < 0)
				offset = 0;
			param_set(param, offset, valuebuf);

		} else {
			for (size_t i = 0; i < (retries > 0 ? retries : 1); i++)
				if (param_push_single(param, offset, valuebuf, 1, (host != INT_MIN ? host : param->node), timeout, paramver) < 0)
					if (i >= retries-1) {
						PyErr_SetString(PyExc_ConnectionError, "No response");
						return -2;
					}
		}

		param_print(param, offset, NULL, 0, 2);

	} else {  // Otherwise; use the queue.

		if (!queue->buffer) {
			if (queue == &param_queue_set)  // We can initialize the global static queue.
				param_queue_init(&param_queue_set, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, paramver);
			else {  // But not dynamically allocated queues, because we can't free them ourselves.
				PyErr_SetString(PyExc_SystemError, "Attempted to add parameter to uninitialized queue");
				return -3;
			}
		}
		if (param_queue_add(queue, param, offset, valuebuf) < 0)
			printf("Queue full\n");

	}

	return 0;
}

/* Private interface for setting the value of an array parameter. */
int _pycsh_util_set_array(param_t *param, PyObject *value, int host, int timeout, int retries) {

	// Transform lazy generators and iterators into sequences,
	// such that their length may be retrieved in a uniform manner.
	// This comes at the expense of memory (and likely performance),
	// especially for very large sequences.
	if (!PySequence_Check(value)) {
		if (PyIter_Check(value)) {
			PyObject * temptuple = PyTuple_Pack(1, value);
			value = PyObject_CallObject((PyObject *)&PyTuple_Type, temptuple);
			Py_DECREF(temptuple);
		} else {
			PyErr_SetString(PyExc_TypeError, "Provided argument must be iterable.");
			return -1;
		}
	} else
		Py_INCREF(value);  // Iterators will be 1 higher than needed so do the same for sequences.

	int seqlen = PySequence_Fast_GET_SIZE(value);

	// We don't support assigning slices (or anything of the like) yet, so...
	if (seqlen != param->array_size) {
		if (param->array_size > 1) {  // Check that the lengths match.
			char buf[120];
			sprintf(buf, "Provided iterable's length does not match parameter's. <iterable length: %i> <param length: %i>", seqlen, param->array_size);
			PyErr_SetString(PyExc_ValueError, buf);
		} else  // Check that the parameter is an array.
			PyErr_SetString(PyExc_TypeError, "Cannot assign iterable to non-array type parameter.");
		Py_DECREF(value);
		return -2;
	}

	// Check that the iterable only contains valid types.
	if (_pycsh_typecheck_sequence(value, _pycsh_misc_param_t_type(param))) {
		Py_DECREF(value);
		return -3;  // Raises TypeError.
	}

	// TODO Kevin: This does not allow for queued operations on array parameters.
	//	This could be implemented by simply replacing 'param_queue_t queue = { };',
	//	with the global queue, but then we need to handle freeing the buffer.
	// TODO Kevin: Also this queue is not used for local parameters (and therefore wasted).
	//	Perhaps structure the function to avoid its unecessary instantiation.
	void * queuebuffer = malloc(PARAM_SERVER_MTU);
	param_queue_t queue = { };
	param_queue_init(&queue, queuebuffer, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, paramver);
	
	for (int i = 0; i < seqlen; i++) {

		PyObject *item = PySequence_Fast_GET_ITEM(value, i);

		if(!item) {
			Py_DECREF(value);
			free(queuebuffer);
			PyErr_SetString(PyExc_RuntimeError, "Iterator went outside the bounds of the iterable.");
			Py_DECREF(value);
			return -4;
		}

		// Set local parameters immediately, use the global queue if autosend if off.
		param_queue_t *usequeue = (!autosend ? &param_queue_set : ((param->node != 0) ? &queue : NULL));
		_pycsh_util_set_single(param, item, i, host, timeout, retries, usequeue);
		
		// 'item' is a borrowed reference, so we don't need to decrement it.
	}

	param_queue_print((autosend ? &queue : &param_queue_set));
	
	if (param->node != 0)
		if (param_push_queue(&queue, 1, param->node, 100, 0) < 0) {  // TODO Kevin: We should probably have a parameter for hwid here.
			PyErr_SetString(PyExc_ConnectionError, "No response.");
			free(queuebuffer);
			Py_DECREF(value);
			return -6;
		}
	
	free(queuebuffer);
	Py_DECREF(value);
	return 0;
}
