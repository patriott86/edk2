[Defines]
PLATFORM_NAME                  = DDTSoft
PLATFORM_GUID                  = 12345678-1234-1234-1234-123456789012
PLATFORM_VERSION               = 1.0
DSC_SPECIFICATION              = 0x00010005
OUTPUT_DIRECTORY               = Build/DDTSoft
SUPPORTED_ARCHITECTURES        = IA32|X64
BUILD_TARGETS                  = DEBUG|RELEASE

[LibraryClasses]
	UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
	UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  	PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  	DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  	BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  	PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  	BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  	RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
  	MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  	UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf

[Components]
	DDTSoft/Application/SelfDestroy/SelfDestroy.inf
