#include "core/selfprotect.h"

#ifdef _WIN32

#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <aclapi.h>

namespace sphone {

void hardenProcessAgainstTermination() {
    // SID do usuario dono do processo (alvo do ACE de negacao).
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return;

    DWORD len = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &len);   // descobre o tamanho
    if (len == 0) { CloseHandle(token); return; }

    auto* tu = static_cast<TOKEN_USER*>(LocalAlloc(LPTR, len));
    if (!tu) { CloseHandle(token); return; }

    PSID userSid = nullptr;
    if (GetTokenInformation(token, TokenUser, tu, len, &len))
        userSid = tu->User.Sid;
    CloseHandle(token);

    if (!userSid || !IsValidSid(userSid)) { LocalFree(tu); return; }

    // DACL atual do processo (para preservar as ACEs existentes).
    PACL oldDacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (GetSecurityInfo(GetCurrentProcess(), SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                        nullptr, nullptr, &oldDacl, nullptr, &sd) != ERROR_SUCCESS) {
        LocalFree(tu);
        return;
    }

    // ACE de NEGACAO de PROCESS_TERMINATE para o usuario. SetEntriesInAcl ordena os
    // DENY antes dos ALLOW, entao essa negacao prevalece sobre o ALLOW padrao.
    EXPLICIT_ACCESSW ea = {};
    ea.grfAccessPermissions = PROCESS_TERMINATE;
    ea.grfAccessMode        = DENY_ACCESS;
    ea.grfInheritance       = NO_INHERITANCE;
    ea.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType  = TRUSTEE_IS_USER;
    ea.Trustee.ptstrName    = static_cast<LPWSTR>(userSid);

    PACL newDacl = nullptr;
    if (SetEntriesInAclW(1, &ea, oldDacl, &newDacl) == ERROR_SUCCESS && newDacl) {
        SetSecurityInfo(GetCurrentProcess(), SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                        nullptr, nullptr, newDacl, nullptr);
        LocalFree(newDacl);
    }

    if (sd) LocalFree(sd);   // oldDacl aponta para dentro de sd; nao liberar separado
    LocalFree(tu);
}

}  // namespace sphone

#else

namespace sphone { void hardenProcessAgainstTermination() {} }

#endif
