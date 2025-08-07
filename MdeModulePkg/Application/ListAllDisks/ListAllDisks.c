#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/BlockIo.h>
#include <Library/DevicePathLib.h>
#include <Library/DevicePathLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/StorageSecurityCommand.h>
#include <Protocol/ScsiIo.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Library/BaseLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/Gpt.h>
#include <Library/BaseMemoryLib.h> // memcmp için
#include <string.h>
#include <Protocol/GraphicsOutput.h>
#include <IndustryStandard/Acpi.h>
#include <Protocol/NvmExpressPassThru.h>   // NVMe için
#include <Protocol/UsbIo.h>                // USB için
#include <Protocol/ScsiPassThru.h>         // SCSI diskler için
#include <Protocol/SdMmcPassThru.h>        // SD / eMMC için

#include <Protocol/NvmExpressPassThru.h>  // NVMe Protokolü
#include <Protocol/AtaPassThru.h>         // SATA / ATA Protokolü
#include <Protocol/UsbIo.h>               // USB Disk Desteği
#include <Protocol/ScsiPassThru.h>        // SCSI Disk Desteği
#include <Protocol/SdMmcPassThru.h>       // SD/MMC Kart Desteği

    //#########################NETWORK #########################################

#include <Protocol/Udp4.h>
#include <Protocol/Ip4.h>
#include <Protocol/ServiceBinding.h>

#define SERVER_IP  {172, 22, 80, 1}  // Sunucu IP adresi
#define SERVER_PORT 9000               // UDP Portu


EFI_STATUS SendUdpBroadcast(CHAR8 *Message, UINT16 Port) {
    EFI_STATUS Status;
    EFI_UDP4_PROTOCOL *Udp4;
    EFI_UDP4_CONFIG_DATA UdpConfig;
    EFI_UDP4_COMPLETION_TOKEN TxToken;
    EFI_UDP4_TRANSMIT_DATA TxData;
    EFI_UDP4_SESSION_DATA UdpSession;
    
    EFI_HANDLE UdpHandle = NULL;
    EFI_SERVICE_BINDING_PROTOCOL *UdpServiceBinding;
    
    EFI_IPv4_ADDRESS LocalIp = { .Addr ={172, 22, 80, 101} };  // Statik IP kullan
    EFI_IPv4_ADDRESS BroadcastIp = { .Addr = {255, 255, 255, 255} };
    EFI_IPv4_ADDRESS SubnetMask = { .Addr = {255, 255, 240, 0} };
    EFI_IPv4_ADDRESS Gateway = { .Addr ={172, 22, 80, 1} };

    Print(L"🔹 SendUdpBroadcast Başlatıldı...\n");

    // **UDP4 Service Binding protokolünü bul**
    Status = gBS->LocateProtocol(&gEfiUdp4ServiceBindingProtocolGuid, NULL, (VOID **)&UdpServiceBinding);
    if (EFI_ERROR(Status)) {
        Print(L"❌ UDP Service Binding protokolü bulunamadı: %r\n", Status);
        return Status;
    }

    Print(L"✅ UDP Service Binding Bulundu!\n");

    // **UDP4 servisini başlat**
    Status = UdpServiceBinding->CreateChild(UdpServiceBinding, &UdpHandle);
    if (EFI_ERROR(Status)) {
        Print(L"❌ UDP4 servisi başlatılamadı: %r\n", Status);
        return Status;
    }

    Print(L"✅ UDP4 Servisi Başlatıldı!\n");

    // **UDP4 protokolünü al**
    Status = gBS->HandleProtocol(UdpHandle, &gEfiUdp4ProtocolGuid, (VOID **)&Udp4);
    if (EFI_ERROR(Status)) {
        Print(L"❌ UDP4 protokolü alınamadı: %r\n", Status);
        return Status;
    }

    Print(L"✅ UDP4 Protokolü Alındı!\n");

    // **UDP4 yapılandırmasını doğru şekilde yap**
    ZeroMem(&UdpConfig, sizeof(EFI_UDP4_CONFIG_DATA));
    UdpConfig.AcceptBroadcast = TRUE;
    UdpConfig.AcceptPromiscuous = FALSE;
    UdpConfig.AcceptAnyPort = FALSE;
    UdpConfig.AllowDuplicatePort = TRUE;
    CopyMem(&UdpConfig.StationAddress, &LocalIp, sizeof(EFI_IPv4_ADDRESS));
    UdpConfig.StationPort = 0;  // 0 = Dinamik port atanır
    CopyMem(&UdpConfig.SubnetMask, &SubnetMask, sizeof(EFI_IPv4_ADDRESS));
    CopyMem(&UdpConfig.RemoteAddress, &Gateway, sizeof(EFI_IPv4_ADDRESS));
    UdpConfig.RemotePort = 0;

    Print(L"🔹 UDP4 Konfigürasyonu Yapılıyor...\n");

    Status = Udp4->Configure(Udp4, &UdpConfig);
    if (EFI_ERROR(Status)) {
        Print(L"❌ UDP4 konfigürasyonu başarısız: %r\n", Status);
        return Status;
    }

    Print(L"✅ UDP4 Başarıyla Konfigüre Edildi...\n");

    // **Broadcast adresini session'a ekle**
    ZeroMem(&UdpSession, sizeof(EFI_UDP4_SESSION_DATA));
    CopyMem(&UdpSession.DestinationAddress, &BroadcastIp, sizeof(EFI_IPv4_ADDRESS));
    UdpSession.DestinationPort = Port;

    // **Gönderilecek mesajı hazırla**
    ZeroMem(&TxData, sizeof(EFI_UDP4_TRANSMIT_DATA));
    //TxData.OverrideData = NULL;  // Override kullanmadığımızı belirtiyoruz
    TxData.UdpSessionData = &UdpSession;
    TxData.DataLength = AsciiStrLen(Message) + 1;

    // **Bellek tahsisi yap (ÖNEMLİ)**
    CHAR8 *MessageBuffer = AllocatePool(AsciiStrLen(Message) + 1);
    if (!MessageBuffer) {
        Print(L"❌ Bellek tahsisi başarısız!\n");
        return EFI_OUT_OF_RESOURCES;
    }
    AsciiStrCpyS(MessageBuffer, AsciiStrLen(Message) + 1, Message);

    TxData.FragmentCount = 1;
    TxData.FragmentTable[0].FragmentLength = TxData.DataLength;
    TxData.FragmentTable[0].FragmentBuffer = (VOID *)MessageBuffer;
    //TxData.FragmentTable[0].FragmentBuffer = MessageBuffer;

    // **Mesajı gönder**
    ZeroMem(&TxToken, sizeof(EFI_UDP4_COMPLETION_TOKEN));
    TxToken.Packet.TxData = &TxData;

    Print(L"🔹 UDP4 Mesajı Gönderiliyor...\n");

    Status = Udp4->Transmit(Udp4, &TxToken);
    if (EFI_ERROR(Status)) {
        Print(L"❌ Broadcast mesajı gönderilemedi: %r\n", Status);
        return Status;
    }

    Print(L"✅ UDP Broadcast mesajı başarıyla gönderildi: %a\n", Message);
    return EFI_SUCCESS;
}





EFI_STATUS SendUdpMessage(CHAR8 *Message) {
    EFI_STATUS Status;
    EFI_UDP4_PROTOCOL *Udp4;
    EFI_UDP4_CONFIG_DATA UdpConfig;
    EFI_IPv4_ADDRESS ServerIp = { .Addr = {172, 22, 80, 1} };

    EFI_UDP4_COMPLETION_TOKEN CompletionToken;
    EFI_UDP4_TRANSMIT_DATA TxData;

    // UDP4 Protokolünü Bul
    Status = gBS->LocateProtocol(&gEfiUdp4ProtocolGuid, NULL, (VOID **)&Udp4);
    if (EFI_ERROR(Status)) {
        Print(L"UDP protokolü bulunamadı: %r\n", Status);
        return Status;
    }

    // UDP Konfigürasyon Ayarları
    UdpConfig.AcceptBroadcast = TRUE;
    UdpConfig.AcceptPromiscuous = FALSE;
    UdpConfig.AcceptAnyPort = FALSE;
    UdpConfig.AllowDuplicatePort = FALSE;
    UdpConfig.TypeOfService = 0;
    UdpConfig.TimeToLive = 128;
    UdpConfig.DoNotFragment = FALSE;
    UdpConfig.ReceiveTimeout = 0;
    UdpConfig.TransmitTimeout = 0;
    UdpConfig.UseDefaultAddress = TRUE;
    UdpConfig.StationAddress.Addr[0] = 172;
    UdpConfig.StationAddress.Addr[1] = 22;
    UdpConfig.StationAddress.Addr[2] = 80;
    UdpConfig.StationAddress.Addr[3] = 1;
    UdpConfig.SubnetMask.Addr[0] = 255;
    UdpConfig.SubnetMask.Addr[1] = 255;
    UdpConfig.SubnetMask.Addr[2] = 255;
    UdpConfig.SubnetMask.Addr[3] = 0;
    UdpConfig.RemotePort = SERVER_PORT;
    UdpConfig.RemoteAddress = ServerIp;

    Status = Udp4->Configure(Udp4, &UdpConfig);
    if (EFI_ERROR(Status)) {
        Print(L"UDP yapılandırması başarısız: %r\n", Status);
        return Status;
    }

    // **Mesajı Gönderme İşlemi**
    TxData.GatewayAddress = NULL;
    TxData.DataLength = AsciiStrLen(Message) + 1;
    TxData.FragmentCount = 1;
    TxData.FragmentTable[0].FragmentBuffer = Message;
    TxData.FragmentTable[0].FragmentLength = TxData.DataLength;

    // Completion Token oluştur
    CompletionToken.Event = NULL;
    CompletionToken.Status = EFI_NOT_READY;
    CompletionToken.Packet.TxData = &TxData;

    // **UDP Mesajını Gönder**
    Status = Udp4->Transmit(Udp4, &CompletionToken);
    if (EFI_ERROR(Status)) {
        Print(L"Mesaj gönderilemedi: %r\n", Status);
    } else {
        Print(L"Mesaj başarıyla gönderildi: %a\n", Message);
    }

    return Status;
}



//#########################NETWORK #########################################



//#########################SPLASH SCREEEN ##################################

#define BACKGROUND_COLOR 0x000000  // Siyah
#define TEXT_COLOR 0xFFFFFF        // Beyaz
#define ACCENT_COLOR 0x0078D7      // Mavi
#define BLUE 0x0000FF      // Mavi

#define DARK_GRAY 0x808080      // koyu gri


// Ekran genişliği ve yüksekliği
UINTN ScreenWidth = 0;
UINTN ScreenHeight = 0;

EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;

VOID PrintColoredText(CHAR16 *Text, UINTN Attribute) {
    // Mevcut renk ayarini sakla
    UINTN OriginalAttribute = gST->ConOut->Mode->Attribute;

    // Yeni renk ayarini uygula
    gST->ConOut->SetAttribute(gST->ConOut, Attribute);

    // Metni yazdir
    Print(Text);

    // Orijinal renk ayarina geri don
    gST->ConOut->SetAttribute(gST->ConOut, OriginalAttribute);
}

VOID PrintColoredTextMK(CHAR16 *Text, UINTN Attribute) {
    // Mevcut renk ayarini sakla
    UINTN OriginalAttribute = gST->ConOut->Mode->Attribute;

    // Yeni renk ayarini uygula
    gST->ConOut->SetAttribute(gST->ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE); 
    // Metni yazdir
    Print(Text);

    // Orijinal renk ayarina geri don
    gST->ConOut->SetAttribute(gST->ConOut, OriginalAttribute);
}

VOID DrawRectangle(UINTN X, UINTN Y, UINTN Width, UINTN Height, UINT32 Color) {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Pixel = {
        .Red = (Color >> 16) & 0xFF,
        .Green = (Color >> 8) & 0xFF,
        .Blue = Color & 0xFF,
        .Reserved = 0
    };
    
    GraphicsOutput->Blt(GraphicsOutput, &Pixel, EfiBltVideoFill, 0, 0, X, Y, Width, Height, 0);
}

VOID ClearScreen(UINT32 Color) {
    DrawRectangle(0, 0, GraphicsOutput->Mode->Info->HorizontalResolution, GraphicsOutput->Mode->Info->VerticalResolution, Color);
}

VOID DrawSplashScreen() {

    // Arka planı siyaha boyayalım
    ClearScreen(BACKGROUND_COLOR);

    // "ASELSAN" logosu gibi bir çubuk ekleyelim
    DrawRectangle(0, 0, ScreenWidth, 180, BLUE);

    // // Yazılar
    // gST->ConOut->SetCursorPosition(gST->ConOut, 10, 5);
    // PrintColoredText(L"ASELSAN\n", EFI_WHITE); // Seçili "Exit" için kırmızı renk
    //Print(L"ASELSAN\n");

UINTN Columns, Rows;
gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &Columns, &Rows); 

// "ASELSAN" kelimesi 7 karakter uzunluğunda
UINTN TextLength = 7; 
UINTN CenterX = (Columns - TextLength) / 2; 

// Metni ortalamak için konumlandır
gST->ConOut->SetCursorPosition(gST->ConOut, CenterX, 0); 
PrintColoredTextMK(L"ASELSAN\n", EFI_WHITE); 



    //gST->ConOut->SetCursorPosition(gST->ConOut, 10, 7);
    CenterX = (Columns - 23) / 2; 

// Metni ortalamak için konumlandır
gST->ConOut->SetCursorPosition(gST->ConOut, CenterX, 2); 

    PrintColoredTextMK(L"Disk Erase Utility v1.0\n", EFI_WHITE); // Seçili "Exit" için kırmızı renk
    //Print(L"Disk Erase Utility v1.0\n");

    // gST->ConOut->SetCursorPosition(gST->ConOut, 10, 9);
        //gST->ConOut->SetCursorPosition(gST->ConOut, 10, 7);
    CenterX = (Columns - 10) / 2; 

// Metni ortalamak için konumlandır
gST->ConOut->SetCursorPosition(gST->ConOut, CenterX, 9); 
    PrintColoredText(L"Loading...\n", EFI_WHITE); // Seçili "Exit" için kırmızı renk
    //Print(L"Loading...");


        // "ASELSAN" logosu gibi bir çubuk ekleyelim
    DrawRectangle(0, ScreenHeight-100   , ScreenWidth, 100, BLUE);

    // Copyright metnini alt çubuğa ekle
//UINTN CopyX = 0; //(ScreenWidth / 2) - (StrLen(L"© 2025 ASELSAN. All Rights Reserved."));
//UINTN CopyY = 22; //ScreenHeight - 100; // Alt çubuğun biraz üstü

CenterX = (Columns - StrLen(L"© 2025 ASELSAN. All Rights Reserved."))/ 2; 

//gST->ConOut->SetAttribute(gST->ConOut, EFI_LIGHTGRAY | EFI_BACKGROUND_BLUE);
gST->ConOut->SetCursorPosition(gST->ConOut, CenterX, Rows-1);
Print(L"© 2025 ASELSAN. All Rights Reserved.");

    gBS->Stall(2000000); // 2 saniye bekle
}

VOID AnimatedLoading() {
    CHAR16 *Frames[] = { L"Loading   ", L"Loading.  ", L"Loading.. ", L"Loading..." };
    UINTN Columns, Rows;
gST->ConOut->QueryMode(gST->ConOut, gST->ConOut->Mode->Mode, &Columns, &Rows); 
    UINTN TextLength = 10; 
UINTN CenterX = (Columns - TextLength) / 2; 
    for (int i = 0; i < 20; i++) { 
        //gST->ConOut->SetCursorPosition(gST->ConOut, 10, 9);
        //PrintColoredText(L"Loading...\n", EFI_WHITE); // Seçili "Exit" için kırmızı renk
        // Metni ortalamak için konumlandır
        gST->ConOut->SetCursorPosition(gST->ConOut, CenterX, 9); 
        //PrintColoredText(L"%s\n", Frames[i % 4], EFI_WHITE); // Seçili "Exit" için kırmızı renk
        Print(L"%s", Frames[i % 4]);
        gBS->Stall(500000); // 500ms bekle
    }
}

//#########################SPLASH SCREEEN ##################################

BOOLEAN IsEqualGuid(EFI_GUID *Guid1, EFI_GUID *Guid2) {
    return (Guid1->Data1 == Guid2->Data1) &&
           (Guid1->Data2 == Guid2->Data2) &&
           (Guid1->Data3 == Guid2->Data3) ;
}

EFI_STATUS ReadPartitions(EFI_BLOCK_IO_PROTOCOL *BlockIo) {
    EFI_STATUS Status;
    UINT8 *Buffer;
    UINTN BufferSize = BlockIo->Media->BlockSize;

    // Sıfır GUID tanımla
    EFI_GUID ZeroGuid = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};

    // GPT başlığını okumak için bellek tahsisi
    Buffer = AllocatePool(BufferSize);
    if (!Buffer) {
        Print(L"Hafıza tahsis edilemedi!\n");
        return EFI_OUT_OF_RESOURCES;
    }

    // GPT başlığını oku (LBA 1)
    Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, 1, BufferSize, Buffer);
    if (EFI_ERROR(Status)) {
        Print(L"GPT başlığı okunamadı: %r\n", Status);
        FreePool(Buffer);
        return Status;
    }

    // GPT başlığını kontrol et
    EFI_PARTITION_TABLE_HEADER *GptHeader = (EFI_PARTITION_TABLE_HEADER *)Buffer;
    if (GptHeader->Header.Signature != EFI_PTAB_HEADER_ID) {
        Print(L"Geçerli bir GPT başlığı bulunamadı.\n");
        FreePool(Buffer);
        return EFI_UNSUPPORTED;
    }

    // Bölüm girişlerini oku
    UINTN PartitionsSize = GptHeader->NumberOfPartitionEntries * GptHeader->SizeOfPartitionEntry;
    UINT8 *PartitionsBuffer = AllocatePool(PartitionsSize);
    if (!PartitionsBuffer) {
        Print(L"Hafıza tahsis edilemedi!\n");
        FreePool(Buffer);
        return EFI_OUT_OF_RESOURCES;
    }

    Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, GptHeader->PartitionEntryLBA, PartitionsSize, PartitionsBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"Bölüm girişleri okunamadı: %r\n", Status);
        FreePool(Buffer);
        FreePool(PartitionsBuffer);
        return Status;
    }

    // Bölüm bilgilerini yazdır
    EFI_PARTITION_ENTRY *PartitionEntry = (EFI_PARTITION_ENTRY *)PartitionsBuffer;
    for (UINTN i = 0; i < GptHeader->NumberOfPartitionEntries; i++) {
        if (IsEqualGuid(&PartitionEntry[i].PartitionTypeGUID, &ZeroGuid)) {
            continue; // Boş bölüm
        }

        //Print(L"  Bölüm %d:\n", i + 1);
        // Print(L"    GUID: %g\n", &PartitionEntry[i].UniquePartitionGUID);
        //Print(L"    Tür: %g\n", &PartitionEntry[i].PartitionTypeGUID);
        // Print(L"    Başlangıç LBA: %ld\n", PartitionEntry[i].StartingLBA);
        // Print(L"    Bitiş LBA: %ld\n", PartitionEntry[i].EndingLBA);
        //Print(L"    Ad: %s\n", PartitionEntry[i].PartitionName);
        Print(L"   Bolüm[%d] Ad: %s\n", i + 1, PartitionEntry[i].PartitionName);
    }

    FreePool(Buffer);
    FreePool(PartitionsBuffer);
    return EFI_SUCCESS;
}

VOID ListPartitions(EFI_HANDLE DiskHandle) {
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    EFI_STATUS Status;

    // BlockIo protokolünü al
    Status = gBS->HandleProtocol(DiskHandle, &gEfiBlockIoProtocolGuid, (void**)&BlockIo);
    if (EFI_ERROR(Status)) {
        Print(L"BlockIo protokolü alınamadı: %r\n", Status);
        return;
    }

    // GPT veya MBR'yi oku ve bölümleri listele
    Status = ReadPartitions(BlockIo);
    if (EFI_ERROR(Status)) {
        Print(L"Bölüm bilgileri okunamadı: %r\n", Status);
    }
}


CHAR16* GetDiskSerialNumber(EFI_HANDLE Handle) {
    EFI_SCSI_IO_PROTOCOL *ScsiIo;
    EFI_ATA_PASS_THRU_PROTOCOL *AtaPassThru;
    EFI_STATUS Status;

    // EFI_SCSI_IO_PROTOCOL'u dene
    Status = gBS->HandleProtocol(Handle, &gEfiScsiIoProtocolGuid, (void**)&ScsiIo);
    if (!EFI_ERROR(Status)) {
        // SCSI komutlari ile seri numarasini al
        return L"SCSI Seri No"; // ornek deger
    }

    // EFI_ATA_PASS_THRU_PROTOCOL'u dene
    Status = gBS->HandleProtocol(Handle, &gEfiAtaPassThruProtocolGuid, (void**)&AtaPassThru);
    if (!EFI_ERROR(Status)) {
        // ATA komutlari ile seri numarasini al
        return L"ATA Seri No"; // ornek deger
    }

    return L"Seri No Yok";
}

CHAR16* GetDiskType(EFI_HANDLE Handle) {
    EFI_STORAGE_SECURITY_COMMAND_PROTOCOL *StorageSecurity;
    EFI_ATA_PASS_THRU_PROTOCOL *AtaPassThru;
    EFI_NVM_EXPRESS_PASS_THRU_PROTOCOL *NvmePassThru;
    EFI_USB_IO_PROTOCOL *UsbIo;
    EFI_SCSI_PASS_THRU_PROTOCOL *ScsiPassThru;
    EFI_SD_MMC_PASS_THRU_PROTOCOL *SdMmcPassThru;
    EFI_STATUS Status;

    // **1️⃣ NVMe Kontrolü**
    Status = gBS->HandleProtocol(Handle, &gEfiNvmExpressPassThruProtocolGuid, (void**)&NvmePassThru);
    if (!EFI_ERROR(Status)) {
        return L"NVMe SSD";
    }

    // **2️⃣ SATA / HDD / SSD Kontrolü**
    Status = gBS->HandleProtocol(Handle, &gEfiAtaPassThruProtocolGuid, (void**)&AtaPassThru);
    if (!EFI_ERROR(Status)) {
        // ATA cihazı HDD veya SATA SSD olabilir
        return L"SATA Disk";
    }

    // **3️⃣ USB Depolama Cihazı Kontrolü**
    Status = gBS->HandleProtocol(Handle, &gEfiUsbIoProtocolGuid, (void**)&UsbIo);
    if (!EFI_ERROR(Status)) {
        return L"USB Disk";
    }

    // **4️⃣ SCSI Disk Kontrolü (Genellikle RAID veya SAS diskler)**
    Status = gBS->HandleProtocol(Handle, &gEfiScsiPassThruProtocolGuid, (void**)&ScsiPassThru);
    if (!EFI_ERROR(Status)) {
        return L"SCSI Disk";
    }

    // **5️⃣ eMMC / SD Kart Kontrolü**
    Status = gBS->HandleProtocol(Handle, &gEfiSdMmcPassThruProtocolGuid, (void**)&SdMmcPassThru);
    if (!EFI_ERROR(Status)) {
        return L"SD / eMMC";
    }

    // **6️⃣ Storage Security Komutları ile SSD olup olmadığını kontrol et**
    Status = gBS->HandleProtocol(Handle, &gEfiStorageSecurityCommandProtocolGuid, (void**)&StorageSecurity);
    if (!EFI_ERROR(Status)) {
        return L"SSD (Güvenlik Komutları)";
    }

    // **7️⃣ Bilinmeyen veya Desteklenmeyen Disk**
    return L"Bilinmeyen Disk Türü";
}





// CHAR16* GetDiskType(EFI_HANDLE Handle) {
//     EFI_STORAGE_SECURITY_COMMAND_PROTOCOL *StorageSecurity;
//     EFI_ATA_PASS_THRU_PROTOCOL *AtaPassThru;
//     EFI_STATUS Status;

//     // EFI_STORAGE_SECURITY_COMMAND_PROTOCOL'u dene
//     Status = gBS->HandleProtocol(Handle, &gEfiStorageSecurityCommandProtocolGuid, (void**)&StorageSecurity);
//     if (!EFI_ERROR(Status)) {
//         // SSD veya HDD turunu belirle
//         return L"SSD"; // ornek olarak SSD donduruyoruz
//     }

//     // EFI_ATA_PASS_THRU_PROTOCOL'u dene
//     Status = gBS->HandleProtocol(Handle, &gEfiAtaPassThruProtocolGuid, (void**)&AtaPassThru);
//     if (!EFI_ERROR(Status)) {
//         // ATA komutlari ile disk turunu belirle
//         return L"HDD"; // ornek olarak HDD donduruyoruz
//     }

//     return L"Bilinmeyen Tur";
// }

CHAR16* GetUserFriendlyDiskName(EFI_DEVICE_PATH_PROTOCOL *DevicePath) {
    EFI_DEVICE_PATH_PROTOCOL *Node;
    CHAR16 *DiskName = NULL;

    // DevicePath'yi dolas
    for (Node = DevicePath; !IsDevicePathEnd(Node); Node = NextDevicePathNode(Node)) {
        // PCI dugumu
        if (DevicePathType(Node) == HARDWARE_DEVICE_PATH && DevicePathSubType(Node) == HW_PCI_DP) {
            DiskName = L"PCI Disk";
            break;
        }
        // USB dugumu
        if (DevicePathType(Node) == MESSAGING_DEVICE_PATH && DevicePathSubType(Node) == MSG_USB_DP) {
            DiskName = L"USB Disk";
            break;
        }
        // SATA dugumu
        if (DevicePathType(Node) == MESSAGING_DEVICE_PATH && DevicePathSubType(Node) == MSG_SATA_DP) {
            DiskName = L"SATA Disk";
            break;
        }
    }

    // Eger ozel bir disk turu bulunamazsa, varsayilan bir isim dondur
    if (DiskName == NULL) {
        DiskName = L"Disk";
    }

    return DiskName;
}

EFI_STATUS ListDisks(EFI_HANDLE **HandleBuffer, UINTN *HandleCount) {
    EFI_STATUS Status;

    // Diskleri bul
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, HandleCount, HandleBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"Diskleri bulma hatası: %r\n", Status);
        return Status;
    }

    // Her disk için bilgileri yazdır
    for (UINTN i = 0; i < *HandleCount; i++) {
        EFI_BLOCK_IO_PROTOCOL *BlockIo;
        gBS->HandleProtocol((*HandleBuffer)[i], &gEfiBlockIoProtocolGuid, (void**)&BlockIo);

        // Disk boyutunu hesapla
        UINT64 DiskSize = (UINT64)BlockIo->Media->LastBlock * BlockIo->Media->BlockSize / (1024 * 1024); // MB cinsinden
        Print(L"Disk %d - Kapasite: %ld MB\n", i, DiskSize);

        // Disk için bölümleri listele
        ListPartitions((*HandleBuffer)[i]);
    }

    return EFI_SUCCESS;
}

// EFI_STATUS ListDisks(EFI_HANDLE **HandleBuffer, UINTN *HandleCount) {
//     EFI_STATUS Status;
    
//     Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, HandleCount, HandleBuffer);
//     if (EFI_ERROR(Status)) {
//         Print(L"Diskleri bulma hata: %r\n", Status);
//         return Status;
//     }

//     // Kullanildiktan sonra bellegi serbest birak
//     FreePool(HandleBuffer);

//     return EFI_SUCCESS;
// }


UINTN SelectDisk(EFI_HANDLE *HandleBuffer, UINTN DiskCount) {
    UINTN SelectedIndex = 0;
    EFI_INPUT_KEY Key;

    while (TRUE) {
            // **İlk ekranı çiz**
    gST->ConOut->ClearScreen(gST->ConOut);
    Print(L"\nBir disk seçin ve Enter'a basın...\n");


        for (UINTN i = 0; i < DiskCount; i++) {
            EFI_BLOCK_IO_PROTOCOL *BlockIo;
            EFI_DEVICE_PATH_PROTOCOL *DevicePath;
            CHAR16 *DeviceName = NULL;
            CHAR16 *DiskType = NULL;
            CHAR16 *SerialNumber = NULL;
            CHAR16 Buffer[256]; // Formatlı metni tutacak tampon

            // BlockIo protokolünü al
            gBS->HandleProtocol(HandleBuffer[i], &gEfiBlockIoProtocolGuid, (void**)&BlockIo);

            // DevicePath protokolünü al
            gBS->HandleProtocol(HandleBuffer[i], &gEfiDevicePathProtocolGuid, (void**)&DevicePath);

            // Disk boyutunu hesapla
            UINT64 DiskSize = (UINT64)BlockIo->Media->LastBlock * BlockIo->Media->BlockSize / (1024 * 1024); // MB cinsinden

            // Disk adını, türünü ve seri numarasını al
            DeviceName = GetUserFriendlyDiskName(DevicePath);
            DiskType = GetDiskType(HandleBuffer[i]);
            SerialNumber = GetDiskSerialNumber(HandleBuffer[i]);

            // if (StrCmp(DiskType, L"Bilinmeyen Disk Türü") == 0) {
            //     continue;  // Bilinmeyen diskleri listelemeyi atla
            // }


            // Formatlı metni tampona yaz
            UnicodeSPrint(Buffer, sizeof(Buffer), L"[%d] Disk - Kapasite: %ld MB, Ad: %s, Tür: %s, Seri No: %s\n", i, DiskSize, DeviceName, DiskType, SerialNumber);

            // Disk bilgilerini renkli olarak yazdır
            if (i == SelectedIndex) {
                PrintColoredText(Buffer, EFI_GREEN); // Seçili disk için yeşil renk
            } else {
                PrintColoredText(Buffer, EFI_WHITE); // Diğer diskler için beyaz renk
            }

            // Seçili disk için bölümleri listele
            if (i == SelectedIndex) {
                //Print(L"  Bölümler:\n");
                ListPartitions(HandleBuffer[i]);
            }
        }

        // "Exit" seçeneğini ekle
        if (SelectedIndex == DiskCount) {
            PrintColoredText(L"> [Exit] Programdan çık\n", EFI_RED); // Seçili "Exit" için kırmızı renk
        } else {
            Print(L"  [Exit] Programdan çık\n");
        }

        // Kullanıcıdan giriş al
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            if (SelectedIndex == DiskCount) {
                return (UINTN)-1; // "Exit" seçildiğinde özel bir değer döndür
            }
            return SelectedIndex;
        }

        if (Key.ScanCode == SCAN_DOWN && SelectedIndex < DiskCount) {
            SelectedIndex++;
        } else if (Key.ScanCode == SCAN_UP && SelectedIndex > 0) {
            SelectedIndex--;
        }

        // Ekranı temizle ve yeniden çiz
        gST->ConOut->SetCursorPosition(gST->ConOut, 0, 2);
    }
}



// UINTN SelectDisk(EFI_HANDLE *HandleBuffer, UINTN DiskCount) {
//     UINTN SelectedIndex = 0;
//     EFI_INPUT_KEY Key;

//     // **İlk ekrani ciz**
//     gST->ConOut->ClearScreen(gST->ConOut);
//     Print(L"\nBir disk secin ve Enter'a basin...\n");

//     while (TRUE) {
//         for (UINTN i = 0; i < DiskCount; i++) {
//             EFI_BLOCK_IO_PROTOCOL *BlockIo;
//             EFI_DEVICE_PATH_PROTOCOL *DevicePath;
//             CHAR16 *DeviceName = NULL;
//             CHAR16 *DiskType = NULL;
//             CHAR16 *SerialNumber = NULL;
//             CHAR16 Buffer[256]; // Formatli metni tutacak tampon

//             // BlockIo protokolunu al
//             gBS->HandleProtocol(HandleBuffer[i], &gEfiBlockIoProtocolGuid, (void**)&BlockIo);

//             // DevicePath protokolunu al
//             gBS->HandleProtocol(HandleBuffer[i], &gEfiDevicePathProtocolGuid, (void**)&DevicePath);

//             // Disk boyutunu hesapla
//             UINT64 DiskSize = (UINT64)BlockIo->Media->LastBlock * BlockIo->Media->BlockSize / (1024 * 1024); // MB cinsinden

//             // Disk adini, turunu ve seri numarasini al
//             DeviceName = GetUserFriendlyDiskName(DevicePath);
//             DiskType = GetDiskType(HandleBuffer[i]);
//             SerialNumber = GetDiskSerialNumber(HandleBuffer[i]);

//             // Formatli metni tampona yaz
//             UnicodeSPrint(Buffer, sizeof(Buffer), L"[%d] Disk: %ld MB, Ad: %s, Tur: %s, Seri No: %s\n", i, DiskSize, DeviceName, DiskType, SerialNumber);

//             // Disk bilgilerini renkli olarak yazdir
//             if (i == SelectedIndex) {
//                 PrintColoredText(L">", EFI_GREEN); 
//                 PrintColoredText(Buffer, EFI_GREEN); // Secili disk icin yesil renk
//             } else {
//                 PrintColoredText(L" ", EFI_GREEN); 
//                 PrintColoredText(Buffer, EFI_WHITE); // Diger diskler icin beyaz renk
//             }
//         }

//         // "Exit" secenegini ekle
//         if (SelectedIndex == DiskCount) {
//             PrintColoredText(L">[Exit] Programdan cik\n", EFI_RED); // Secili "Exit" icin kirmizi renk
//         } else {
//             Print(L" [Exit] Programdan cik\n");
//         }

//         // Kullanicidan giris al
//         gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
//         gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

//         if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
//             if (SelectedIndex == DiskCount) {
//                 return (UINTN)-1; // "Exit" secildiginde ozel bir deger dondur
//             }
//             return SelectedIndex;
//         }

//         if (Key.ScanCode == SCAN_DOWN && SelectedIndex < DiskCount) {
//             SelectedIndex++;
//         } else if (Key.ScanCode == SCAN_UP && SelectedIndex > 0) {
//             SelectedIndex--;
//         }

//         // Ekrani temizle ve yeniden ciz
//         gST->ConOut->SetCursorPosition(gST->ConOut, 0, 2);
//     }
// }

// UINTN SelectDisk(EFI_HANDLE *HandleBuffer, UINTN DiskCount) {
//     UINTN SelectedIndex = 0;
//     EFI_INPUT_KEY Key;

//     // **İlk ekrani ciz**
//     gST->ConOut->ClearScreen(gST->ConOut);
//     Print(L"\nBir disk secin ve Enter'a basin...\n");

//     while (TRUE) {
//         for (UINTN i = 0; i < DiskCount; i++) {
//             EFI_BLOCK_IO_PROTOCOL *BlockIo;
//             EFI_DEVICE_PATH_PROTOCOL *DevicePath;
//             CHAR16 *DeviceName = NULL;

//             // BlockIo protokolunu al
//             gBS->HandleProtocol(HandleBuffer[i], &gEfiBlockIoProtocolGuid, (void**)&BlockIo);

//             // DevicePath protokolunu al
//             gBS->HandleProtocol(HandleBuffer[i], &gEfiDevicePathProtocolGuid, (void**)&DevicePath);

//             // Disk boyutunu hesapla
//             UINT64 DiskSize = (UINT64)BlockIo->Media->LastBlock * BlockIo->Media->BlockSize / (1024 * 1024); // MB cinsinden

//             // Disk adini al
//             DeviceName = GetUserFriendlyDiskName(DevicePath);

//             // Disk bilgilerini ekrana yazdir
//             if (i == SelectedIndex) {
//                 Print(L"> [%d] Disk - Kapasite: %ld MB, Ad: %s\n", i, DiskSize, DeviceName);
//             } else {
//                 Print(L"  [%d] Disk - Kapasite: %ld MB, Ad: %s\n", i, DiskSize, DeviceName);
//             }
//         }

//         // Kullanicidan giris al
//         gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
//         gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

//         if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) { 
//             return SelectedIndex;
//         }

//         if (Key.ScanCode == SCAN_DOWN && SelectedIndex < DiskCount - 1) {
//             SelectedIndex++;
//         } else if (Key.ScanCode == SCAN_UP && SelectedIndex > 0) {
//             SelectedIndex--;
//         }

//         // Ekrani temizle ve yeniden ciz
//         gST->ConOut->SetCursorPosition(gST->ConOut, 0, 2);
//     }
// }

// UINTN SelectDisk(EFI_HANDLE *HandleBuffer, UINTN DiskCount) {
//     UINTN SelectedIndex = 0;
//     EFI_INPUT_KEY Key;

//     // **İlk ekrani ciz**
//     gST->ConOut->ClearScreen(gST->ConOut);
//     Print(L"\nBir disk secin ve Enter'a basin...\n");

//     while (TRUE) {
//         for (UINTN i = 0; i < DiskCount; i++) {
//             EFI_BLOCK_IO_PROTOCOL *BlockIo;
//             EFI_DEVICE_PATH_PROTOCOL *DevicePath;
//             CHAR16 *DeviceName = NULL;

//             // BlockIo protokolunu al
//             gBS->HandleProtocol(HandleBuffer[i], &gEfiBlockIoProtocolGuid, (void**)&BlockIo);

//             // DevicePath protokolunu al
//             gBS->HandleProtocol(HandleBuffer[i], &gEfiDevicePathProtocolGuid, (void**)&DevicePath);

//             // Disk boyutunu hesapla
//             UINT64 DiskSize = (UINT64)BlockIo->Media->LastBlock * BlockIo->Media->BlockSize / (1024 * 1024); // MB cinsinden

//             // DevicePath'den disk adini al (ornegin, "VenHw(1234-5678)")
//             DeviceName = ConvertDevicePathToText(DevicePath, FALSE, FALSE);

//             // Disk bilgilerini ekrana yazdir
//             if (i == SelectedIndex) {
//                 Print(L"> [%d] Disk - Kapasite: %ld MB, Ad: %s\n", i, DiskSize, DeviceName);
//             } else {
//                 Print(L"  [%d] Disk - Kapasite: %ld MB, Ad: %s\n", i, DiskSize, DeviceName);
//             }

//             // Bellek temizleme
//             if (DeviceName) {
//                 FreePool(DeviceName);
//             }
//         }

//         // Kullanicidan giris al
//         gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
//         gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

//         if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) { 
//             return SelectedIndex;
//         }

//         if (Key.ScanCode == SCAN_DOWN && SelectedIndex < DiskCount - 1) {
//             SelectedIndex++;
//         } else if (Key.ScanCode == SCAN_UP && SelectedIndex > 0) {
//             SelectedIndex--;
//         }

//         // Ekrani temizle ve yeniden ciz
//         gST->ConOut->SetCursorPosition(gST->ConOut, 0, 2);
//     }
// }

// UINTN SelectDisk(EFI_HANDLE *HandleBuffer, UINTN DiskCount) {
//     UINTN SelectedIndex = 0;
//     EFI_INPUT_KEY Key;

//     // **İlk ekrani ciz**
//     gST->ConOut->ClearScreen(gST->ConOut);
//     Print(L"\nBir disk secin ve Enter'a bas...\n");

//     while (TRUE) {
//         for (UINTN i = 0; i < DiskCount; i++) {
//             EFI_BLOCK_IO_PROTOCOL *BlockIo;
//             gBS->HandleProtocol(HandleBuffer[i], &gEfiBlockIoProtocolGuid, (void**)&BlockIo);
            
//             UINT64 DiskSize = (UINT64)BlockIo->Media->LastBlock * BlockIo->Media->BlockSize / (1024 * 1024);
            
//             if (i == SelectedIndex) {
//                 Print(L"> [%d] Disk - Kapasite: %ld MB\n", i, DiskSize);
//             } else {
//                 Print(L"  [%d] Disk - Kapasite: %ld MB\n", i, DiskSize);
//             }
//         }

//         // Kullanicidan giris al
//         gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
//         gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

//         if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) { 
//             return SelectedIndex;
//         }

//         if (Key.ScanCode == SCAN_DOWN && SelectedIndex < DiskCount - 1) {
//             SelectedIndex++;
//         } else if (Key.ScanCode == SCAN_UP && SelectedIndex > 0) {
//             SelectedIndex--;
//         }

//         gST->ConOut->SetCursorPosition(gST->ConOut, 0, 2);
//     }
// }

VOID ErasingAnimation() {
    CHAR16 *Frames[] = { L"-", L"\\", L"|", L"/" };
    for (int i = 0; i < 20; i++) {
        gST->ConOut->SetCursorPosition(gST->ConOut, 10, 10);
        Print(L"Erasing... %s", Frames[i % 4]);
        gBS->Stall(200000);
    }
}

EFI_STATUS SecureEraseDisk(EFI_BLOCK_IO_PROTOCOL *BlockIo) {
    UINTN BufferSize = BlockIo->Media->BlockSize * 1024; // Daha buyuk bloklarla silme
    VOID *Buffer = AllocateZeroPool(BufferSize);
    if (!Buffer) {
        Print(L"Hafiza tahsis edilemedi!\n");
        return EFI_OUT_OF_RESOURCES;
    }

    for (UINT64 i = 0; i < BlockIo->Media->LastBlock; i += 1024) {
        EFI_STATUS Status = BlockIo->WriteBlocks(BlockIo, BlockIo->Media->MediaId, i, BufferSize, Buffer);
        if (EFI_ERROR(Status)) {
            Print(L"Blok %d silinemedi: %r\n", i, Status);
        }
    }

    FreePool(Buffer);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    EFI_STATUS Status;


    // SendUdpMessage("Selam!");

    // for (int i = 0; i < 20; i++)
    // {
    //     // Broadcast mesajını gönder (SERVER_PORT numaralı porta)
    //     Status = SendUdpBroadcast("UEFI UDP Broadcast Mesajı", SERVER_PORT);
    //     Print(L"SendUdpBroadcast send status: %r\n", Status);

    //     // 5 saniye bekle
    //     gBS->Stall(500000);
    // }
    



    // Graphics Output Protocol'ü al
    Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID**)&GraphicsOutput);
    if (EFI_ERROR(Status)) {
        Print(L"Graphics Output Protocol bulunamadı! %r\n", Status);
        return Status;
    }

    // Ekran genişliği ve yüksekliği
    ScreenWidth = GraphicsOutput->Mode->Info->HorizontalResolution;
    ScreenHeight = GraphicsOutput->Mode->Info->VerticalResolution;

    DrawSplashScreen();

    AnimatedLoading();

    // 5 saniye bekle
    gBS->Stall(5000000);



    while (TRUE) {

        // Broadcast mesajını gönder (SERVER_PORT numaralı porta)
        Status = SendUdpBroadcast("UEFI UDP Broadcast Mesajı", SERVER_PORT);

        if (EFI_ERROR(ListDisks(&HandleBuffer, &HandleCount))) {
            return EFI_ABORTED;
        }

        UINTN SelectedDisk = SelectDisk(HandleBuffer, HandleCount);

        // "Exit" secildiginde programdan cik
        if (SelectedDisk == (UINTN)-1) {
            Print(L"\nProgram Kapatiliyor...\n");
            return EFI_SUCCESS;
        }

        EFI_BLOCK_IO_PROTOCOL *BlockIo;
        gBS->HandleProtocol(HandleBuffer[SelectedDisk], &gEfiBlockIoProtocolGuid, (void**)&BlockIo);

        Print(L"\nSecilen disk silinecek! Devam etmek icin Enter'a basin...");
        EFI_INPUT_KEY Key;
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
        if (Key.UnicodeChar != CHAR_CARRIAGE_RETURN) {
            continue;
        }

        ErasingAnimation();
        SecureEraseDisk(BlockIo);

        Print(L"\nDisk basariyla silindi! Ana menuye donmek icin bir tusa basin...\n");
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, NULL);
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    }

    return EFI_SUCCESS;
}