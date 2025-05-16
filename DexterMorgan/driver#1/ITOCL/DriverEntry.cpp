#include <ntifs.h>

extern "C" {
	NTKERNELAPI NTSTATUS IoCreateDriver(PUNICODE_STRING Drivername,
		                                PDRIVER_INITIALIZE InitialiyationFunction);

	NTKERNELAPI NTSTATUS MmCopyVirtualMemory(PEPROCESS SourceProcess, PVOID SourceAddress,
		PEPROCESS TargetProcess, PVOID TargetAddress,
		SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode,
		PSIZE_T ReturnSize);
}
void debug_print(PCSTR text) {
	KdPrint((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, text));
}

namespace driver {
	namespace codes {
		constexpr ULONG attach =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

	    constexpr ULONG read =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

    	constexpr ULONG write =
			CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	}

	struct Request {
		HANDLE process_id;

		PVOID target;
		PVOID buffer;

		SIZE_T size;
		SIZE_T return_size;
	};

	NTSTATUS create(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);
		IoCompleteRequest(irp, IO_NO_INCREMENT);

		return irp->IoStatus.Status;
	}

	NTSTATUS close(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return irp->IoStatus.Status;
	}

	NTSTATUS device_control(PDEVICE_OBJECT device_object, PIRP irp) {
		UNREFERENCED_PARAMETER(device_object);
		debug_print("[+] Device Control Called fr\n");
		NTSTATUS status = STATUS_UNSUCCESSFUL;
		PIO_STACK_LOCATION stack_irp = IoGetCurrentIrpStackLocation(irp);
		auto request = reinterpret_cast<Request*>(irp->AssociatedIrp.SystemBuffer);
		// Important or BSOD
		if (stack_irp == nullptr || request == nullptr) {
			IoCompleteRequest(irp, IO_NO_INCREMENT);
			return status;
		}
		static PEPROCESS target_process = nullptr;
		const ULONG control_code = stack_irp->Parameters.DeviceIoControl.IoControlCode;
		switch (control_code) {
		case driver::codes::attach:
			status = PsLookupProcessByProcessId(request->process_id, &target_process);
			break;

		case driver::codes::read:
			if (target_process == nullptr)
				status = MmCopyVirtualMemory(PsGetCurrentProcess(), request->buffer,
					target_process, request->target,
					request->size, KernelMode, &request->return_size);
			break;

		case driver::codes::write:
			if (target_process == nullptr)
				status = MmCopyVirtualMemory(target_process, request->target,
					PsGetCurrentProcess(), request->buffer,
					request->size, KernelMode, &request->return_size);
			break;
		}
		irp->IoStatus.Status = status;
		irp->IoStatus.Information = sizeof(Request);
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return status;
		// till here
	}
}// drivernamespace

NTSTATUS driver_main(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path) {
	UNREFERENCED_PARAMETER(registry_path);
	UNICODE_STRING device_name = {};
	RtlInitUnicodeString(&device_name, L"\\Driver\\driver#1");
	PDEVICE_OBJECT device_object = nullptr;
	NTSTATUS status = IoCreateDevice(driver_object, 0, &device_name, FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN, FALSE, &device_object);

	if (status == STATUS_SUCCESS) {
		debug_print("[-] Failed to Create driver device\n");
		return status;
	}
	
	debug_print("[+] Driver Device Created\n");

	UNICODE_STRING sybolic_link = {};
	RtlInitUnicodeString(&sybolic_link, L"\\DosDevices\\driver#1");

	status = IoCreateSymbolicLink(&sybolic_link, &device_name);
	if (status == STATUS_SUCCESS) {
		debug_print("[-] Failed to Create Symbolic link\n");
		return status;
	}
	debug_print("[+] Driver symbolic link Created\n");

	SetFlag(device_object->Flags, DO_BUFFERED_IO);
	driver_object->MajorFunction[IRP_MJ_CREATE] = driver::create;
	driver_object->MajorFunction[IRP_MJ_CLOSE] = driver::close;
	driver_object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver::device_control;
    // Clear
	ClearFlag(device_object->Flags, DO_DEVICE_INITIALIZING);
	debug_print("Driver Intialized\n");

     return status;

}
NTSTATUS DriverEntry() {
	debug_print("[+] Driver loaded");

	UNICODE_STRING driver_name = {};
	RtlInitUnicodeString(&driver_name, L"\\Driver\\driver#1");
	return IoCreateDriver(&driver_name, &driver_main);
}