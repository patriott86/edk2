#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>

#include <Protocol/GraphicsOutput.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#define MAX_DISKS 10
#define DISK_WIDTH  400
#define DISK_HEIGHT 50
#define START_Y     100

EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;

EFI_STATUS InitGraphics() {
    EFI_STATUS Status;

    Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&GraphicsOutput);
    if (EFI_ERROR(Status)) {
        Print(L"Grafik protokolü bulunamadı!\n");
        return Status;
    }

    GraphicsOutput->SetMode(GraphicsOutput, 0); // İlk grafik moduna geçiş
    return EFI_SUCCESS;
}

// VOID ClearScreen(UINT32 Color) {
//     EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
//     EFI_STATUS Status;

//     Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop);
//     if (EFI_ERROR(Status)) {
//         Print(L"Graphics Output Protocol bulunamadı!\n");
//         return;
//     }

//     // Ekranı istenilen renkle doldur
//     Gop->Blt(Gop, &Color, EfiBltVideoFill, 0, 0, 0, 0, 
//              Gop->Mode->Info->HorizontalResolution, 
//              Gop->Mode->Info->VerticalResolution, 0);
// }


VOID ClearScreen(UINT32 Color) {
    UINTN Width = GraphicsOutput->Mode->Info->HorizontalResolution;
    UINTN Height = GraphicsOutput->Mode->Info->VerticalResolution;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *ScreenBuffer = AllocateZeroPool(Width * Height * sizeof(EFI_GRAPHICS_OUTPUT_BLT_PIXEL));

    for (UINTN i = 0; i < Width * Height; i++) {
        ScreenBuffer[i].Red = (Color >> 16) & 0xFF;
        ScreenBuffer[i].Green = (Color >> 8) & 0xFF;
        ScreenBuffer[i].Blue = (Color)&0xFF;
    }

    GraphicsOutput->Blt(GraphicsOutput, ScreenBuffer, EfiBltBufferToVideo, 0, 0, 0, 0, Width, Height, 0);
    FreePool(ScreenBuffer);
}


VOID ListDisks() {
    EFI_STATUS Status;
    UINTN HandleCount;
    EFI_HANDLE *HandleBuffer;
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    
    // Uygun olan tüm cihazları al
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &HandleBuffer);
    if (EFI_ERROR(Status)) {
        Print(L"Diskler bulunamadı!\n");
        return;
    }

    Print(L"\nDiskler:\n");
    for (UINTN Index = 0; Index < HandleCount; Index++) {
        Status = gBS->HandleProtocol(HandleBuffer[Index], &gEfiBlockIoProtocolGuid, (VOID **)&BlockIo);
        if (EFI_ERROR(Status)) {
            continue;
        }

        // Disk kapasitesini hesapla (MB cinsinden)
        UINT64 DiskSize = (UINT64)BlockIo->Media->LastBlock + 1;
        DiskSize *= BlockIo->Media->BlockSize;
        DiskSize /= (1024 * 1024);

        // Disk bilgilerini yazdır
        Print(L"Disk %d: Kapasite: %ld MB\n", Index, DiskSize);
    }
    
    FreePool(HandleBuffer);
}


VOID DrawDiskMenu(UINTN SelectedIndex, UINTN HandleCount) {
    ClearScreen(0x202020); // Arka planı koyu gri yap

    ListDisks();


    for (UINTN i = 0; i < HandleCount; i++) {
        EFI_GRAPHICS_OUTPUT_BLT_PIXEL Color = {255, 255, 255, 0}; // Beyaz

        if (i == SelectedIndex) {
            Color.Red = 0;
            Color.Green = 255;
            Color.Blue = 0; // Seçili olan disk yeşil olacak
        }

        GraphicsOutput->Blt(GraphicsOutput, &Color, EfiBltVideoFill, 0, 0, 200, START_Y + i * (DISK_HEIGHT + 10), DISK_WIDTH, DISK_HEIGHT, 0);
    }
}

VOID PrintAt(UINTN X, UINTN Y, CHAR16 *Format, ...) {
    // Terminali X, Y konumuna taşımak için kaçış dizisini kullan
    Print(L"\033[%d;%dH", Y, X);
    
    VA_LIST Marker;
    VA_START(Marker, Format);
    Print(Format, Marker);
    VA_END(Marker);
}


UINTN SelectDisk(UINTN DiskCount) {
    UINTN SelectedIndex = 0;
    EFI_INPUT_KEY Key;

    while (TRUE) {
        DrawDiskMenu(SelectedIndex, DiskCount);

        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) { // Enter
            break;
        } else if (Key.ScanCode == SCAN_DOWN && SelectedIndex < DiskCount - 1) {
            SelectedIndex++;
        } else if (Key.ScanCode == SCAN_UP && SelectedIndex > 0) {
            SelectedIndex--;
        }
    }

    return SelectedIndex;
}


VOID ErasingAnimation() {
    CHAR16 *Frames[] = { L".  ", L".. ", L"..." };
    UINTN X = 300, Y = 300; // Animasyonun konumu

    for (int i = 0; i < 20; i++) { // 20 kez döndür
        ClearScreen(0x000000); // Arka planı siyah yap
        PrintAt(X, Y, L"Erasing%s", Frames[i % 3]);
        gBS->Stall(200000); // 200ms bekle
    }
}


EFI_STATUS SecureEraseDisk(EFI_BLOCK_IO_PROTOCOL *BlockIo) {
    UINTN BufferSize = BlockIo->Media->BlockSize;
    VOID *Buffer = AllocateZeroPool(BufferSize);
    if (!Buffer) {
        Print(L"Hafıza tahsis edilemedi!\n");
        return EFI_OUT_OF_RESOURCES;
    }

    for (UINT64 i = 0; i < BlockIo->Media->LastBlock; i++) {
        EFI_STATUS Status = BlockIo->WriteBlocks(BlockIo, BlockIo->Media->MediaId, i, BufferSize, Buffer);
        if (EFI_ERROR(Status)) {
            Print(L"Blok %d silinemedi: %r\n", i, Status);
        }
    }

    FreePool(Buffer);
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    
    Status = InitGraphics();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    while (TRUE) {
        // Diskleri listele
        Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiBlockIoProtocolGuid, NULL, &HandleCount, &HandleBuffer);
        if (EFI_ERROR(Status)) {
            Print(L"Disk bulunamadı!\n");
            return EFI_ABORTED;
        }

        // Kullanıcıdan seçim al
        UINTN SelectedDisk = SelectDisk(HandleCount);
        EFI_BLOCK_IO_PROTOCOL *BlockIo;
        gBS->HandleProtocol(HandleBuffer[SelectedDisk], &gEfiBlockIoProtocolGuid, (void**)&BlockIo);

        // Kullanıcıdan onay al
        ClearScreen(0x000000); // Ekranı temizle
        PrintAt(300, 250, L"Disk silinecek! Devam etmek için Enter'a basın...");
        EFI_INPUT_KEY Key;
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
        if (Key.UnicodeChar != CHAR_CARRIAGE_RETURN) {
            continue;
        }

        // Silme animasyonu göster
        ErasingAnimation();

        // Diski güvenli bir şekilde sil
        SecureEraseDisk(BlockIo);

        // İşlem tamamlandığında mesaj ver
        ClearScreen(0x000000);
        PrintAt(300, 250, L"Disk başarıyla silindi! Ana menüye dönmek için Enter'a bas...");
        gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    }

    return EFI_SUCCESS;
}

