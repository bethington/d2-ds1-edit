#ifdef WIN32

// C does not get IFileOpenDialog helper macros by default; enabling
// COBJMACROS gives us IFileOpenDialog_Show() style wrappers instead of
// the C++ -> operator.
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <objbase.h>
#include <string.h>

#include "ui/win_folder_picker.h"

// UTF-8 -> UTF-16 into a caller-provided buffer. Returns 1 on success.
static int utf8_to_wide(const char *s, wchar_t *out, int out_cap)
{
   int n;
   if (s == NULL || out == NULL || out_cap < 1) return 0;
   n = MultiByteToWideChar(CP_UTF8, 0, s, -1, out, out_cap);
   return (n > 0) ? 1 : 0;
}

int win_pick_folder(const char *title, const char *initial_path,
                    char *out_utf8, int out_cap)
{
   IFileOpenDialog *dlg = NULL;
   IShellItem      *item = NULL;
   IShellItem      *initial_item = NULL;
   PWSTR            path_w = NULL;
   DWORD            options = 0;
   HRESULT          hr;
   int              rc = 0;
   int              need_uninit = 0;
   wchar_t          wbuf[1024];

   if (out_utf8 == NULL || out_cap < 2) return 0;
   out_utf8[0] = 0;

   // COM init. S_FALSE == already initialised the same way; both require a
   // matching CoUninitialize. RPC_E_CHANGED_MODE means someone else set an
   // incompatible mode -- we can't use the dialog in that case.
   hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
   if (hr == S_OK || hr == S_FALSE)
      need_uninit = 1;
   else if (hr != RPC_E_CHANGED_MODE)
      return 0;

   hr = CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                         &IID_IFileOpenDialog, (void **) &dlg);
   if (FAILED(hr) || dlg == NULL) goto done;

   // Folder-only, require real filesystem paths (no virtual shell items),
   // use the modern "Force filesystem" layout with the New Folder button.
   IFileOpenDialog_GetOptions(dlg, &options);
   IFileOpenDialog_SetOptions(dlg, options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

   if (title != NULL && title[0] != 0 &&
       utf8_to_wide(title, wbuf, sizeof(wbuf) / sizeof(wbuf[0])))
   {
      IFileOpenDialog_SetTitle(dlg, wbuf);
   }

   if (initial_path != NULL && initial_path[0] != 0 &&
       utf8_to_wide(initial_path, wbuf, sizeof(wbuf) / sizeof(wbuf[0])))
   {
      if (SUCCEEDED(SHCreateItemFromParsingName(wbuf, NULL,
                                                &IID_IShellItem,
                                                (void **) &initial_item)))
      {
         IFileOpenDialog_SetFolder(dlg, initial_item);
         IShellItem_Release(initial_item);
         initial_item = NULL;
      }
   }

   hr = IFileOpenDialog_Show(dlg, NULL);
   if (FAILED(hr)) goto done;   // user cancel = HRESULT_FROM_WIN32(ERROR_CANCELLED)

   hr = IFileOpenDialog_GetResult(dlg, &item);
   if (FAILED(hr) || item == NULL) goto done;

   hr = IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path_w);
   if (SUCCEEDED(hr) && path_w != NULL)
   {
      int n = WideCharToMultiByte(CP_UTF8, 0, path_w, -1,
                                  out_utf8, out_cap, NULL, NULL);
      if (n > 0) rc = 1;
      CoTaskMemFree(path_w);
   }

done:
   if (item != NULL) IShellItem_Release(item);
   if (dlg  != NULL) IFileOpenDialog_Release(dlg);
   if (need_uninit)  CoUninitialize();
   return rc;
}

#endif // WIN32
