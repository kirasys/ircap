#pragma once

UNICODE_STRING TARGET_DEVICE_NAME = RTL_CONSTANT_STRING(L"\\Driver\\tmusa");
UNICODE_STRING PROGRAM_FILE_PATH  = RTL_CONSTANT_STRING(L"\\DosDevices\\C:\\program.irp");

//
// Target device driver declarations.
//
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH hookDispatchDeviceControl;

PDRIVER_DISPATCH g_oriDispatchDeviceControl;

PDRIVER_OBJECT  g_targetDriverObject;

HANDLE g_handle = NULL;

VOID
ThreadIRPHooker(
	_In_ PVOID Context
)
/*++
Routine Description:
	This is the main thread that hooks DispatchDeviceControl of the target device.

Arguments:
	Context     -- pointer to the device object
--*/
{
	UNREFERENCED_PARAMETER(Context);
	int i = 0;

	LARGE_INTEGER Time;
	NTSTATUS ntStatus;
	OBJECT_ATTRIBUTES ObjectAttributes;
	IO_STATUS_BLOCK IoStatusBlock;

	Time.QuadPart = -10000000;	// 1 second

	for (i = 0; i < 10000; i++) {
		KDPRINTF("[ircap.sys] try hooking (%d) \n", i);

		g_targetDriverObject = GetDriverObjectbyDeviceName(TARGET_DEVICE_NAME);
		if (g_targetDriverObject)
			break;

		KeDelayExecutionThread(KernelMode, FALSE, &Time);
	}

	g_oriDispatchDeviceControl = g_targetDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
	g_targetDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = hookDispatchDeviceControl;

	InitializeObjectAttributes(&ObjectAttributes, &PROGRAM_FILE_PATH, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
	ntStatus = ZwCreateFile(&g_handle, GENERIC_WRITE, &ObjectAttributes,
		&IoStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OVERWRITE_IF,
		FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

	if (!NT_SUCCESS(ntStatus)) {
		KDPRINTF("[ircap.sys] Creating program.irp error\n");
		return;
	}
}

NTSTATUS
hookDispatchDeviceControl(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
)
/*++
Routine Description:
	This routine is called by the I/O system to perform a device I/O
	control function.
Arguments:
	DeviceObject - a pointer to the object that represents the device
		that I/O is to be done on.
	Irp - a pointer to the I/O Request Packet for this request.
Return Value:
	NT status code
--*/
{
	PIO_STACK_LOCATION  irpSp;// Pointer to current stack location
	ULONG               inBufLength; // Input buffer length
	ULONG               outBufLength; // Output buffer length
	PCHAR               inBuf;		  // pointer to Input

	IO_STATUS_BLOCK		IoStatusBlock;
	UINT32				programInfo[3];

	PAGED_CODE();

	irpSp = IoGetCurrentIrpStackLocation(Irp);
	inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
	outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

	if (irpSp->Parameters.DeviceIoControl.IoControlCode && METHOD_NEITHER == METHOD_NEITHER)
		inBuf = irpSp->Parameters.DeviceIoControl.Type3InputBuffer;
	else
		inBuf = Irp->AssociatedIrp.SystemBuffer;

	KDPRINTF("[ircap.sys] hookDispatchDeviceControl: IoControlCode (%x) InBufferLength (%d) InBufferLength (%d)\n",
		irpSp->Parameters.DeviceIoControl.IoControlCode, inBufLength, outBufLength);

	programInfo[0] = irpSp->Parameters.DeviceIoControl.IoControlCode;
	programInfo[1] = inBufLength;
	programInfo[2] = outBufLength;

	ZwWriteFile(g_handle, NULL, NULL, NULL, &IoStatusBlock, (PVOID)programInfo, sizeof(programInfo), NULL, NULL);
	ZwWriteFile(g_handle, NULL, NULL, NULL, &IoStatusBlock, (PVOID)inBuf, inBufLength, NULL, NULL);

	return g_oriDispatchDeviceControl(DeviceObject, Irp);
}