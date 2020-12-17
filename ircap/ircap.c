
#include <ntddk.h>

#include "ircap.h"
#include "helpers.h"
#include "hook.h"

#define NT_DEVICE_NAME      L"\\Device\\ircap"
#define DOS_DEVICE_NAME     L"\\DosDevices\\ircap"

//
// Device driver routine declarations.
//

DRIVER_INITIALIZE DriverEntry;

_Dispatch_type_(IRP_MJ_CREATE)
_Dispatch_type_(IRP_MJ_CLOSE)
DRIVER_DISPATCH IioctlCreateClose;

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH IioctlDeviceControl;

DRIVER_UNLOAD IioctlUnloadDriver;

#ifdef ALLOC_PRAGMA
#pragma alloc_text( INIT, DriverEntry )
#pragma alloc_text( PAGE, IioctlCreateClose)
#pragma alloc_text( PAGE, IioctlDeviceControl)
#pragma alloc_text( PAGE, IioctlUnloadDriver)
#endif // ALLOC_PRAGMA

NTSTATUS
DriverEntry(
	_In_ PDRIVER_OBJECT	DriverObject,
	_In_ PUNICODE_STRING	RegistryPath
	)
/*++
	Routine Description:
		This routine is called by the Operating System to initialize the driver.
		It creates the device object, fills in the dispatch entry points and
		completes the initialization.
	Arguments:
		DriverObject - a pointer to the object that represents this device
		driver.
		RegistryPath - a pointer to our Services key in the registry.
	Return Value:
		STATUS_SUCCESS if initialized; an error otherwise.
--*/ 
{
	NTSTATUS        ntStatus;
	UNICODE_STRING  ntUnicodeString;		// NT Device Name
	UNICODE_STRING  ntWin32NameString;		// Win32 Name
	PDEVICE_OBJECT  deviceObject = NULL;    // ptr to device object
	HANDLE          threadHandle;
	PVOID			threadObject;

	UNREFERENCED_PARAMETER(RegistryPath);

	/* Create a thread to hook DispatchDeviceControl of the target device */
	KDPRINTF("[ircap.sys] Create a thread for hooking\n");
	ntStatus = PsCreateSystemThread(&threadHandle,
		THREAD_ALL_ACCESS,
		NULL,
		(HANDLE)0,
		NULL,
		ThreadIRPHooker,
		NULL);

	if (!NT_SUCCESS(ntStatus))
	{
		KDPRINTF("[ircap.sys] PsCreateSystemThread failed.\n");
		return ntStatus;
	}

	ObReferenceObjectByHandle(threadHandle,
		0,
		NULL,
		KernelMode,
		&threadObject,
		NULL);
	KeWaitForSingleObject(threadObject, Executive, KernelMode, FALSE, NULL);

	ObDereferenceObject(threadObject);
	ZwClose(threadHandle);


	/* Create an IOCTL interface to communicate with agent  */
	KDPRINTF("[ircap.sys] Create an IOCTL interface %S", NT_DEVICE_NAME);
	RtlInitUnicodeString(&ntUnicodeString, NT_DEVICE_NAME);
	ntStatus = IoCreateDevice(
		DriverObject,                   // Our Driver Object
		0,                              // We don't use a device extension
		&ntUnicodeString,               // Device name
		FILE_DEVICE_UNKNOWN,            // Device type
		FILE_DEVICE_SECURE_OPEN,		// Device characteristics
		FALSE,                          // Not an exclusive device
		&deviceObject);					// Returned ptr to Device Object

	if (!NT_SUCCESS(ntStatus)) {
		KDPRINTF("[ircap.sys] IoCreateDevice failed.\n");
		IoDeleteDevice(DriverObject->DeviceObject);
		return ntStatus;
	}

	DriverObject->MajorFunction[IRP_MJ_CREATE] = IioctlCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = IioctlCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IioctlDeviceControl;
	DriverObject->DriverUnload = IioctlUnloadDriver;

	//
	// Initialize a Unicode String containing the Win32 name
	// for our device.
	//

	RtlInitUnicodeString(&ntWin32NameString, DOS_DEVICE_NAME);

	//
	// Create a symbolic link between our device name  and the Win32 name
	//

	ntStatus = IoCreateSymbolicLink(
		&ntWin32NameString, &ntUnicodeString);

	if (!NT_SUCCESS(ntStatus))
	{
		//
		// Delete everything that this routine has allocated.
		//
		KDPRINTF("[ircap.sys] IoCreateSymbolicLink failed.\n");
		IoDeleteDevice(deviceObject);
	}

	return ntStatus;
}

NTSTATUS
IioctlCreateClose(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
)
/*++
Routine Description:
	This routine is called by the I/O system when the SIOCTL is opened or
	closed.
	No action is performed other than completing the request successfully.
Arguments:
	DeviceObject - a pointer to the object that represents the device
	that I/O is to be done on.
	Irp - a pointer to the I/O Request Packet for this request.
Return Value:
	NT status code
--*/

{
	UNREFERENCED_PARAMETER(DeviceObject);

	PAGED_CODE();

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

VOID
IioctlUnloadDriver(
	_In_ PDRIVER_OBJECT DriverObject
)
/*++
Routine Description:
	This routine is called by the I/O system to unload the driver.
	Any resources previously allocated must be freed.
Arguments:
	DriverObject - a pointer to the object that represents our driver.
Return Value:
	None
--*/

{
	PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
	UNICODE_STRING uniWin32NameString;

	PAGED_CODE();

	//
	// Unhook DispatchDeviceControl of the target device.
	//

	if (g_oriDispatchDeviceControl)
		g_targetDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = g_oriDispatchDeviceControl;

	if (g_handle)
		ZwClose(g_handle);

	//
	// Create counted string version of our Win32 device name.
	//

	RtlInitUnicodeString(&uniWin32NameString, DOS_DEVICE_NAME);

	//
	// Delete the link from our device name to a name in the Win32 namespace.
	//

	IoDeleteSymbolicLink(&uniWin32NameString);

	if (deviceObject != NULL)
		IoDeleteDevice(deviceObject);
}

NTSTATUS
IioctlDeviceControl(
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
    NTSTATUS            ntStatus = STATUS_SUCCESS; // Assume success
    ULONG               inBufLength; // Input buffer length
    ULONG               outBufLength; // Output buffer length
    PCHAR               inBuf, outBuf; // pointer to Input and output buffer

    UNREFERENCED_PARAMETER(DeviceObject);

    PAGED_CODE();

    irpSp = IoGetCurrentIrpStackLocation(Irp);
    inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

    if (!inBufLength || !outBufLength)
    {
        ntStatus = STATUS_INVALID_PARAMETER;
        goto End;
    }

    //
    // Determine which I/O control code was specified.
    //

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_SIOCTL_METHOD_BUFFERED:

        //
        // In this method the I/O manager allocates a buffer large enough to
        // to accommodate larger of the user input buffer and output buffer,
        // assigns the address to Irp->AssociatedIrp.SystemBuffer, and
        // copies the content of the user input buffer into this SystemBuffer
        //

        KDPRINTF("[ircap.sys] Called IOCTL_SIOCTL_METHOD_BUFFERED\n");

        //
        // Input buffer and output buffer is same in this case, read the
        // content of the buffer before writing to it
        //

        inBuf = Irp->AssociatedIrp.SystemBuffer;
        outBuf = Irp->AssociatedIrp.SystemBuffer;

        //
        // Read the data from the buffer
        //

        //
        // We are using the following function to print characters instead
        // DebugPrint with %s format because we string we get may or
        // may not be null terminated.
        //
        //PrintChars(inBuf, inBufLength);

        //
        // Write to the buffer over-writes the input buffer content
        //

        //RtlCopyBytes(outBuf, data, outBufLength);

        //SIOCTL_KDPRINT(("\tData to User : "));
        //PrintChars(outBuf, datalen);

        //
        // Assign the length of the data copied to IoStatus.Information
        // of the Irp and complete the Irp.
        //

        //Irp->IoStatus.Information = (outBufLength < datalen ? outBufLength : datalen);

        //
        // When the Irp is completed the content of the SystemBuffer
        // is copied to the User output buffer and the SystemBuffer is
        // is freed.
        //

        break;

    default:

        //
        // The specified I/O control code is unrecognized by this driver.
        //

        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
		KDPRINTF("[ircap.sys] ERROR: unrecognized IOCTL %x\n",
            irpSp->Parameters.DeviceIoControl.IoControlCode);
        break;
    }

End:
    //
    // Finish the I/O operation by simply completing the packet and returning
    // the same status as in the packet itself.
    //

    Irp->IoStatus.Status = ntStatus;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return ntStatus;
}