#include "rp_object.h"
#include "stdio.h"

#include "rp_rubypyobj.h"
#include "rp_function.h"

RUBY_EXTERN VALUE mRubyPythonBridge;
RUBY_EXTERN VALUE ePythonError;
RUBY_EXTERN VALUE cRubyPyObject;
RUBY_EXTERN VALUE cBlankObject;
RUBY_EXTERN VALUE cRubyPyClass;
RUBY_EXTERN VALUE cRubyPyFunction;

VALUE cRubyPyModule;
VALUE cRubyPyInstance;


VALUE rpInstanceFromPyObject(PyObject* pInst)
{
	PObj* self;
	PyObject *pClassDict,*pClass,*pInstDict;
	VALUE rInstDict, rClassDict;	
	VALUE rInst = rb_class_new_instance(0, NULL, cRubyPyInstance);

	Data_Get_Struct(rInst, PObj, self);
	self->pObject = pInst;
	
	pClass = PyObject_GetAttrString(pInst,"__class__");
	pClassDict = PyObject_GetAttrString(pClass,"__dict__");
	pInstDict = PyObject_GetAttrString(pInst,"__dict__");
	
	Py_XINCREF(pClassDict);
	Py_XINCREF(pInstDict);
	
	rClassDict = rpObjectFromPyObject(pClassDict);
	rInstDict = rpObjectFromPyObject(pInstDict);
	
	rb_iv_set(rInst,"@pclassdict", rClassDict);
	rb_iv_set(rInst,"@pinstdict", rInstDict);
	
	return rInst;
}

static
VALUE rpInstanceSetAttr(VALUE self, VALUE args)
{
	VALUE name, name_string, rClassDict, result, rInstDict;
	VALUE ret;
	
	int instance;
	char* cname;
	
	PObj *pClassDict,*pInstDict,*pDict;
	
	PyObject* pName;
	
	name = rb_ary_shift(args);
	name_string = rb_funcall(name, rb_intern("to_s"), 0);
	
	rb_funcall(name_string, rb_intern("chop!"), 0);
	
	if(!rpHasSymbol(self, name_string))
	{		
		int argc;		
		VALUE* argv;
		
		argc = RARRAY_LEN(args);
		argv = ALLOC_N(VALUE, argc);
		MEMCPY(argv, RARRAY_PTR(args), VALUE, argc);
		return rb_call_super(argc, argv);
	}
	
	cname = STR2CSTR(name_string);
	
	if((NUM2INT(rb_funcall(args, rb_intern("size"), 0)) == 1))
	{
		args = rb_ary_entry(args, 0);
	}
	
	rClassDict = rb_iv_get(self,"@pclassdict");
	rInstDict = rb_iv_get(self,"@pinstdict");
	
	Data_Get_Struct(rClassDict, PObj, pClassDict);
	Data_Get_Struct(rInstDict, PObj, pInstDict);
	
	pName = PyString_FromString(cname);
	
	if(PyDict_Contains(pInstDict->pObject, pName))
	{
		pDict = pInstDict;

	}
	else
	{
		pDict = pClassDict;
		
	}
	
	Py_XDECREF(pName);
	PyDict_SetItemString(pDict->pObject, STR2CSTR(name_string), rtopObject(args, 0));
	
	return Qtrue;
}

//:nodoc:
static
VALUE rpInstanceDelegate(VALUE self, VALUE args)
{
	VALUE name, name_string, rClassDict, result, rInstDict;
	VALUE ret;
	char* cname;
	PObj *pClassDict,*pInstDict;
	PyObject* pCalled;
	
	if(rpSymbolIsSetter(args))
	{
		return rpInstanceSetAttr(self, args);
	}
	
	if(!rpHasSymbol(self, rb_ary_entry(args, 0)))
	{		
		int argc;
		VALUE* argv;
		
		argc = RARRAY_LEN(args);
		argv = ALLOC_N(VALUE, argc);
		MEMCPY(argv, RARRAY_PTR(args), VALUE, argc);
		return rb_call_super(argc, argv);
	}
	
	name = rb_ary_shift(args);
	name_string = rb_funcall(name, rb_intern("to_s"), 0);
	cname = STR2CSTR(name_string);
		
	rClassDict = rb_iv_get(self,"@pclassdict");
	rInstDict = rb_iv_get(self,"@pinstdict");
	
	Data_Get_Struct(rClassDict, PObj, pClassDict);
	Data_Get_Struct(rInstDict, PObj, pInstDict);
	
	pCalled = PyDict_GetItemString(pInstDict->pObject, cname);
	
	if(!pCalled)
	{
		pCalled = PyDict_GetItemString(pClassDict->pObject, cname);
	}
	
	Py_XINCREF(pCalled);
	result = ptorObjectKeep(pCalled);
	
	if(rb_obj_is_instance_of(result, cRubyPyFunction))
	{
		Py_XINCREF(rpObjectGetPyObject(self));
		rb_ary_unshift(args, self);
		ret = rpCall(pCalled, args);
		return ret;
	}
	
	return result;
	
}


VALUE rpObjectWrap(PyObject* pObj)
{
	VALUE rObj;
	
	if(PyFunction_Check(pObj)||PyMethod_Check(pObj)||!PyObject_HasAttrString(pObj,"__dict__"))
	{
		return rpFunctionFromPyObject(pObj);

	}
	
	if(PyInstance_Check(pObj))
	{
		rObj = rpInstanceFromPyObject(pObj);
		return rObj;
	}
	
	return rpClassFromPyObject(pObj);
}


//Pass this function the argument list for a function call. Checks to see
//if the first parameter (the method symbol name here because this is called
//from within method_missing) ends with an equals
int rpSymbolIsSetter(VALUE args)
{
	VALUE mname;
	VALUE name_string;
	VALUE isSetter_;
	
	mname = rb_ary_entry(args, 0);
	name_string = rb_funcall(mname, rb_intern("to_s"), 0);
	
	isSetter_ = rb_funcall(name_string, rb_intern("end_with?"), 1, rb_str_new2("="));
	
	return Qtrue == isSetter_;
}



//Tests if the first argument ends with !!. See the comment for
//rpSymbolIsSetter
int rpSymbolIsDoubleBang(VALUE args)
{
	VALUE mname = rb_ary_entry(args, 0);
	VALUE name_string = rb_funcall(mname, rb_intern("to_s"), 0);
	
	return Qtrue == rb_funcall(name_string, rb_intern("end_with?"), 1, rb_str_new2("!!"));
}


void Init_RubyPyInstance()
{
	cRubyPyInstance = rb_define_class_under(mRubyPythonBridge,"RubyPyInstance", cRubyPyObject);
	rb_define_method(cRubyPyInstance,"method_missing", rpInstanceDelegate,- 2);
}
