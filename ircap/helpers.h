#pragma once

#if DBG
#define KDPRINTF(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, __VA_ARGS__)

#else
#define SIOCTL_KDPRINT(_x_)
#endif

extern POBJECT_TYPE* IoDriverObjectType;

typedef NTSTATUS(*OB_REFERENCE_OBJECTBYNAME)
(
	IN PUNICODE_STRING ObjectName,
	IN ULONG Attributes,
	IN PACCESS_STATE PassedAccessState OPTIONAL,
	IN ACCESS_MASK DesiredAccess OPTIONAL,
	IN POBJECT_TYPE ObjectType,
	IN KPROCESSOR_MODE AccessMode,
	IN OUT PVOID ParseContext OPTIONAL,
	OUT PVOID* Object
	);
OB_REFERENCE_OBJECTBYNAME ObReferenceObjectByName_func;
UNICODE_STRING sObReferenceObjectByName = RTL_CONSTANT_STRING(L"ObReferenceObjectByName");

PDRIVER_OBJECT target_driverObj;

PDRIVER_OBJECT GetDriverObjectbyDeviceName(UNICODE_STRING DeviceName)
{
	PVOID  pDriverObj;
	NTSTATUS status;

	ObReferenceObjectByName_func = (OB_REFERENCE_OBJECTBYNAME)MmGetSystemRoutineAddress(&sObReferenceObjectByName);
	if (ObReferenceObjectByName_func) {
		status = ObReferenceObjectByName_func(
			&DeviceName,    // IN PUNICODE_STRING ObjectName
			OBJ_CASE_INSENSITIVE,  // IN ULONG Attributes
			NULL,      // IN PACCESS_STATE PassedAccessState OPTIONAL, 
			(ACCESS_MASK)0L,
			*IoDriverObjectType,
			KernelMode,
			NULL,
			&pDriverObj
		);

		if (NT_SUCCESS(status))
		{
			ObDereferenceObject(pDriverObj);
			return (PDRIVER_OBJECT)pDriverObj;
		}
	}
	return (PDRIVER_OBJECT)0;
}