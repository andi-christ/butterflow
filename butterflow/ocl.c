#include <Python.h>
#include <stdio.h>
#if defined(__APPLE__) && defined(__MACH__)
    #include <OpenCL/opencl.h>
#else
    #include <CL/cl.h>
#endif

#define cl_safe(A) if((A) != CL_SUCCESS) { \
    PyErr_SetString(PyExc_RuntimeError, "opencl call failed"); \
    return (PyObject*)NULL; }
#define MIN_CL_VER 1.2
#define MIN_WORK_GROUP_SIZE 256
#define MIN_FB_UPDATEMATRICES_WORK_ITEM_SIZE        {32,  8, 1}
#define MIN_FB_BOXFILTER5_WORK_ITEM_SIZE            {256, 1, 1}
#define MIN_FB_UPDATEFLOW_WORK_ITEM_SIZE            {32,  8, 1}
#define MIN_FB_GAUSSIANBLUR_WORK_ITEM_SIZE          {256, 1, 1}
#define MIN_FB_POLYNOMIALEXPANSION_WORK_ITEM_SIZE   {256, 1, 1}
#define MIN_FB_GAUSSIANBLUR5_WORK_ITEM_SIZE         {256, 1, 1}

/* max of each work item size columns*/
static const int MIN_FB_WORK_ITEM_SIZES[3] = {256, 8, 1};


static int ocl_device_is_compatible(char *d_vers, char *d_prof, char *p_prof,
  size_t d_max_work_group_size, size_t *d_max_work_item_sizes);
static PyObject* compat_ocl_device_available(PyObject *self, PyObject *noargs);

static PyObject*
print_ocl_devices(PyObject *self, PyObject *noargs) {
    cl_platform_id platforms[32];
    cl_uint n_platforms;
    cl_device_id *devices;
    cl_uint n_devices;
    char p_name[1024];
    char p_vend[1024];
    char p_vers[1024];
    char p_prof[1024];
    char d_name[1024];
    char d_vers[1024];
    char d_dver[1024];
    char d_prof[1024];
    size_t d_max_work_group_size;
    size_t d_max_work_item_sizes[3];

    cl_safe(clGetPlatformIDs(32, platforms, &n_platforms));

    if (n_platforms <= 0) {
        printf("No OpenCL devices detected. Please check your OpenCL "
               "installation.\n");
        Py_RETURN_NONE;
    }

    printf("OpenCL devices:");

    for (int i = 0; i < n_platforms; i++) {
        cl_platform_id p = platforms[i];
        cl_safe(clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, 1024,
                                  p_prof, NULL));
        cl_safe(clGetPlatformInfo(p, CL_PLATFORM_NAME, 1024, p_name, NULL));
        cl_safe(clGetPlatformInfo(p, CL_PLATFORM_VENDOR,  1024, p_vend, NULL));
        cl_safe(clGetPlatformInfo(p, CL_PLATFORM_VERSION, 1024, p_vers, NULL));
        printf("\n  Platform          \t: %s"
               "\n  Platform Vendor   \t: %s"
               "\n  Platform Version  \t: %s", p_name, p_vend, p_vers);

        cl_safe(clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL,
                               &n_devices));

        devices = (cl_device_id*)calloc(sizeof(cl_device_id), n_devices);
        cl_safe(clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, n_devices,
                               devices, NULL));

        for (int j = 0; j < n_devices; j++) {
            cl_device_id d = devices[j];
            cl_safe(clGetDeviceInfo(d, CL_DEVICE_PROFILE, 1024, d_prof, NULL));
            cl_safe(clGetDeviceInfo(d, CL_DEVICE_NAME, 1024, d_name, NULL));
            cl_safe(clGetDeviceInfo(d, CL_DEVICE_VERSION, 1024, d_vers, NULL));
            cl_safe(clGetDeviceInfo(d, CL_DRIVER_VERSION, 1024, d_dver, NULL));
            cl_safe(clGetDeviceInfo(d, CL_DEVICE_MAX_WORK_GROUP_SIZE,
                                    sizeof(d_max_work_group_size),
                                    &d_max_work_group_size, NULL));
            cl_safe(clGetDeviceInfo(d, CL_DEVICE_MAX_WORK_ITEM_SIZES,
                                    sizeof(d_max_work_item_sizes),
                                    &d_max_work_item_sizes, NULL));
            int compatible = ocl_device_is_compatible(d_vers,
                                                      d_prof,
                                                      p_prof,
                                  d_max_work_group_size, d_max_work_item_sizes);
            char *compatible_string = "Yes";
            if (!compatible) {
              compatible_string = "No";
            }
            printf("\n    Device     \t\t: %s"
                   "\n      Version  \t\t: %s"
                   "\n      Version  \t\t: %s"
                   "\n      Work Size\t\t: %d, %dx%dx%d"
                   "\n      Compatible\t: %s",
                   d_name, d_vers, d_dver, d_max_work_group_size,
                   d_max_work_item_sizes[0],
                   d_max_work_item_sizes[1],
                   d_max_work_item_sizes[2],
                   compatible_string);
        }
        free(devices);
    }
    printf("\n");
    Py_RETURN_NONE;
}

static int
ocl_device_is_compatible(char *d_vers, char *d_prof, char *p_prof,
  size_t d_max_work_group_size, size_t *d_max_work_item_sizes) {
  int compatible = 1;

  char *p = strtok(d_vers, "OpenCL ");
  float cl_version = atof(p);

  compatible &= cl_version >= MIN_CL_VER;
  compatible &= strcmp(d_prof, "FULL_PROFILE") == 0;
  compatible &= strcmp(p_prof, "FULL_PROFILE") == 0;
  compatible &= d_max_work_group_size    >= MIN_WORK_GROUP_SIZE;
  compatible &= d_max_work_item_sizes[0] >= MIN_FB_WORK_ITEM_SIZES[0];
  compatible &= d_max_work_item_sizes[1] >= MIN_FB_WORK_ITEM_SIZES[1];
  compatible &= d_max_work_item_sizes[2] >= MIN_FB_WORK_ITEM_SIZES[2];

  return compatible;
}

static PyObject*
compat_ocl_device_available(PyObject *self, PyObject *noargs) {
    cl_platform_id platforms[32];
    cl_uint n_platforms;
    cl_device_id *devices;
    cl_uint n_devices;
    char p_prof[1024];
    char d_vers[1024];
    char d_prof[1024];
    size_t d_max_work_group_size;
    size_t d_max_work_item_sizes[3];

    cl_safe(clGetPlatformIDs(32, platforms, &n_platforms));

    for (int i = 0; i < n_platforms; i++) {
        cl_safe(clGetPlatformInfo(platforms[i], CL_PLATFORM_PROFILE, 1024,
                                  p_prof, NULL));

        cl_safe(clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL,
                               &n_devices));

        devices = (cl_device_id*)calloc(sizeof(cl_device_id), n_devices);
        cl_safe(clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, n_devices,
                               devices, NULL));

        for (int j = 0; j < n_devices; j++) {
            cl_device_id d = devices[j];

            cl_safe(clGetDeviceInfo(d, CL_DEVICE_VERSION, 1024, d_vers, NULL));
            cl_safe(clGetDeviceInfo(d, CL_DEVICE_PROFILE, 1024, d_prof, NULL));
            cl_safe(clGetDeviceInfo(d, CL_DEVICE_MAX_WORK_GROUP_SIZE,
                                    sizeof(d_max_work_group_size),
                                    &d_max_work_group_size, NULL));
            cl_safe(clGetDeviceInfo(d, CL_DEVICE_MAX_WORK_ITEM_SIZES,
                                    sizeof(d_max_work_item_sizes),
                                    &d_max_work_item_sizes, NULL));

            if (ocl_device_is_compatible(d_vers, d_prof, p_prof,
                                        d_max_work_group_size,
                                        d_max_work_item_sizes)) {
                free(devices);
                Py_RETURN_TRUE;
            }
        }
        free(devices);
    }
    Py_RETURN_FALSE;
}

static PyMethodDef module_methods[] = {
    {"compat_ocl_device_available", compat_ocl_device_available, METH_NOARGS,
        "Checks if a compatible ocl device is available"},
    {"print_ocl_devices", print_ocl_devices, METH_NOARGS,
        "Prints all available OpenCL devices"},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
initocl(void) {
    (void) Py_InitModule("ocl", module_methods);
}
