#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextOut.h>

// Ekranın kenarlarına çerçeve çizmek için fonksiyon
VOID DrawBorder(EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop) {
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL White = {255, 255, 255, 0}; // Beyaz renk
    UINTN Width = Gop->Mode->Info->HorizontalResolution;
    UINTN Height = Gop->Mode->Info->VerticalResolution;

    // Üst kenar
    Gop->Blt(Gop, &White, EfiBltVideoFill, 0, 0, 0, 0, Width, 5, 0);

    // Alt kenar
    Gop->Blt(Gop, &White, EfiBltVideoFill, 0, 0, 0, Height - 5, Width, 5, 0);

    // Sol kenar
    Gop->Blt(Gop, &White, EfiBltVideoFill, 0, 0, 0, 0, 5, Height, 0);

    // Sağ kenar
    Gop->Blt(Gop, &White, EfiBltVideoFill, 0, 0, Width - 5, 0, 5, Height, 0);
}

EFI_STATUS
EFIAPI
UefiMain (
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
    )
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;

    // GOP (Graphics Output Protocol) bul
    Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (void**)&Gop);
    if (EFI_ERROR(Status)) {
        Print(L"GOP bulunamadı: %r\n", Status);
        return Status;
    }

    // Ekranı maviye boya
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL Blue = {0, 0, 255, 0}; // Mavi renk
    UINTN Width = Gop->Mode->Info->HorizontalResolution;
    UINTN Height = Gop->Mode->Info->VerticalResolution;
    Gop->Blt(Gop, &Blue, EfiBltVideoFill, 0, 0, 0, 0, Width, Height, 0);

    // Çerçeve çiz
    DrawBorder(Gop);

    // Metin rengini beyaz yap
    ConOut = SystemTable->ConOut;
    ConOut->SetAttribute(ConOut, EFI_WHITE | EFI_BACKGROUND_BLUE);

    // Metni ekrana yazdır
    ConOut->SetCursorPosition(ConOut, 10, 10);
    Print(L"Hello, World!\n");

    // Kullanıcı bir tuşa basana kadar bekle
    SystemTable->BootServices->Stall(5000000); // 5 saniye bekle

    return EFI_SUCCESS;
}