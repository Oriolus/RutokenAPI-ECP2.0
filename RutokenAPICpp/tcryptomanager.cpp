#include "tcryptomanager.h"

pkcs11_core::crypto::TCryptoManager::TCryptoManager(device::TokenSession *tSession, TKeyManager *keyManager)
{
    this->tSession = tSession;
    this->pFunctionList = (CK_FUNCTION_LIST_PTR)tSession->GetFunctionListPtr();
    this->hSession = (CK_SESSION_HANDLE)tSession->GetSessionHandle();
    this->keyManager = keyManager;
}

pkcs11_core::crypto::TCryptoManager::~TCryptoManager()
{

}

void pkcs11_core::crypto::TCryptoManager::preCheck()
{
    if(this->pFunctionList == nullptr)
        throw new TException("Function list not loaded", Error::FUNCITON_LIST_NOT_LOADED);
    if(this->hSession == NULL_PTR)
        throw new TException("There is not opened session", Error::SESSION_HANDLE_INVALID);
    if(this->tSession == nullptr)
        throw new TException("Unknown error at TCryptoManager", Error::SESSION_EXISTS);
}

byte_array pkcs11_core::crypto::TCryptoManager::GetRandom(const int32_t size)
{
    CK_BYTE_PTR bpRandom = getRandom(size);
    byte_array result((byte*)bpRandom, (byte*)bpRandom + size);
    delete[] bpRandom;
    return result;
}

CK_BYTE_PTR pkcs11_core::crypto::TCryptoManager::getRandom(const int32_t size)
{
    preCheck();
    CK_BYTE_PTR result = nullptr;
    result = new CK_BYTE[size];
    CK_RV rv = pFunctionList->C_GenerateRandom(hSession, result, size);
    if(rv != CKR_OK)
    {
        delete[] result;
        throw new TException("Can't get random", (Error)rv);
    }
    return result;
}

byte_array pkcs11_core::crypto::TCryptoManager::Digest_Gost3411_94(const byte_array &plaintext)
{
    return sDigest(plaintext, 32, CKM_GOSTR3411);
}

byte_array pkcs11_core::crypto::TCryptoManager::Digest_Gost3411_12_256(const byte_array &plaintext)
{
    return sDigest(plaintext, 32, CKM_GOSTR3411_12_256);
}

byte_array pkcs11_core::crypto::TCryptoManager::Digest_Gost3411_12_512(const byte_array &plaintext)
{
    return sDigest(plaintext, 64, CKM_GOSTR3411_12_512);
}

bool pkcs11_core::crypto::TCryptoManager::IsValidDigest_Gost3411_94(const byte_array &plaintext, const byte_array digest)
{
    byte_array digest1 = Digest_Gost3411_94(plaintext);
    return digest1 == digest;
}

bool pkcs11_core::crypto::TCryptoManager::IsValidDigest_Gost3411_12_256(const byte_array &plaintext, const byte_array digest)
{
    byte_array digest1 = Digest_Gost3411_12_256(plaintext);
    return digest1 == digest;
}

bool pkcs11_core::crypto::TCryptoManager::IsValidDigest_Gost3411_12_512(const byte_array &plaintext, const byte_array digest)
{
    byte_array digest1 = Digest_Gost3411_12_512(plaintext);
    return digest1 == digest;
}

byte_array pkcs11_core::crypto::TCryptoManager::sDigest(const byte_array &plaintext, const uint64_t lDigestSize, const CK_MECHANISM_TYPE digestMech)
{
    uint64_t lPlainTextSize = plaintext.size();
    uint64_t lTmpDigestSize = lDigestSize;
    CK_BYTE_PTR bpDigest = digest((CK_BYTE_PTR)plaintext.data(), lPlainTextSize, &lTmpDigestSize, digestMech);
    byte_array result((byte*)bpDigest, (byte*)bpDigest + lTmpDigestSize);
    memset(bpDigest, 0x00, lDigestSize);
    delete[] bpDigest;
    return  result;
}

CK_BYTE_PTR pkcs11_core::crypto::TCryptoManager::digest(const CK_BYTE_PTR bpPlaintext, const uint64_t lPlaintextSize,
                                   uint64_t *lDigestSize,
                                   const CK_MECHANISM_TYPE digestMech)
{
    preCheck();
    CK_MECHANISM mDigestMech = { digestMech, NULL_PTR, 0 };
    CK_RV rv = pFunctionList->C_DigestInit(hSession, &mDigestMech);
    if(rv != CKR_OK)
    {
        throw new TException("Can't initialize digect calculation", (Error)rv);
    }

    uint64_t lTmpDigestSize = *lDigestSize;
    CK_BYTE_PTR bpTmpDigest = new CK_BYTE[lTmpDigestSize];
    rv = pFunctionList->C_Digest(hSession, bpPlaintext, lPlaintextSize, bpTmpDigest, (CK_ULONG*)(&lTmpDigestSize));
    if(rv != CKR_OK)
    {
        delete[] bpTmpDigest;
        throw new TException("Can't calculate digest", (Error)rv);
    }
    *lDigestSize = lTmpDigestSize;
    return bpTmpDigest;
}

byte_array pkcs11_core::crypto::TCryptoManager::Encrypt_Gost28147(const byte_array &keyID, const byte_array &plaintext, const byte_array *IV)
{
    return sEncrypt(keyID, plaintext, IV, CKM_GOST28147, CKO_SECRET_KEY);
}

byte_array pkcs11_core::crypto::TCryptoManager::Encrypt_Gost28147_ECB(const byte_array &keyID, byte_array &plaintext)
{
    while(plaintext.size() % 8 != 0)
        plaintext.push_back((byte)0);
    return sEncrypt(keyID, plaintext, nullptr, CKM_GOST28147_ECB, CKO_SECRET_KEY);
}

byte_array pkcs11_core::crypto::TCryptoManager::Decrypt_Gost28147_ECB(const byte_array &keyID, const byte_array &ciphertext)
{
    return sDecrypt(keyID, ciphertext, nullptr, CKM_GOST28147_ECB, CKO_SECRET_KEY);
}


CK_BYTE_PTR pkcs11_core::crypto::TCryptoManager::encrypt(const CK_OBJECT_HANDLE hKey,
                                    CK_BYTE_PTR bpPlaintext, const CK_ULONG lPlaintextSize,
                                    CK_BYTE_PTR bpIV, const CK_ULONG lIVSize,
                                    uint64_t *lCiphertextSize,
                                    const CK_MECHANISM_TYPE encMechType)
{
    preCheck();

    CK_MECHANISM encMech = { encMechType, bpIV, lIVSize };
    CK_RV rv = pFunctionList->C_EncryptInit(hSession, &encMech, hKey);
    if(rv != CKR_OK)
        throw new TException("Can't initialize encrypt operation", (Error)rv);

    CK_ULONG lfCiphertextSize = 0;
    rv = pFunctionList->C_Encrypt(hSession, bpPlaintext, lPlaintextSize, NULL_PTR, &lfCiphertextSize);
    if(rv != CKR_OK)
        throw new TException("Can't get ciphertext size", (Error)rv);

    CK_BYTE_PTR bpCiphertext = new CK_BYTE[lfCiphertextSize];
    memset(bpCiphertext, 0x00, lfCiphertextSize);
    rv = pFunctionList->C_Encrypt(hSession, bpPlaintext, lPlaintextSize, bpCiphertext, &lfCiphertextSize);
    if(rv != CKR_OK)
    {
        delete[] bpCiphertext;
        throw new TException("Can't enctypt plaintext", (Error)rv);
    }

    *lCiphertextSize = (uint64_t)lfCiphertextSize;
    lfCiphertextSize = 0;

    return bpCiphertext;
}

byte_array pkcs11_core::crypto::TCryptoManager::sEncrypt(const byte_array &keyID, const byte_array &plaintext, const byte_array *IV, const CK_MECHANISM_TYPE encMechType, const CK_OBJECT_CLASS keyClass)
{
    preCheck();
    std::vector<CK_OBJECT_HANDLE> keysHandles = this->keyManager->getKeyHandle(keyID, keyClass);
    if(keysHandles.size() == 0)
        throw new TException("Can't find keys with such id", Error::KEY_HANDLE_HOT_FOUND);

    uint64_t ciphertextSize = 0;
    CK_BYTE_PTR ciphertext = nullptr;
    if(IV != nullptr) ciphertext = encrypt(keysHandles[0], (CK_BYTE_PTR)plaintext.data(), (CK_ULONG)plaintext.size(), (CK_BYTE_PTR)IV->data(), (CK_ULONG)IV->size(), &ciphertextSize, encMechType);
    else ciphertext = encrypt(keysHandles[0], (CK_BYTE_PTR)plaintext.data(), (CK_ULONG)plaintext.size(), NULL_PTR, 0, &ciphertextSize, encMechType);
    byte_array result((byte*)ciphertext, (byte*)ciphertext + ciphertextSize);
    delete[] ciphertext;
    return result;
}

byte_array pkcs11_core::crypto::TCryptoManager::Decrypt_Gost28147(const byte_array &keyID, const byte_array &ciphertext, const byte_array *IV)
{
    return sDecrypt(keyID, ciphertext, IV, CKM_GOST28147, CKO_SECRET_KEY);
}

CK_BYTE_PTR pkcs11_core::crypto::TCryptoManager::decrypt(const CK_OBJECT_HANDLE hKey,
                                    const CK_BYTE_PTR bpCiphertext, const CK_ULONG lCiphertextSize,
                                    const CK_BYTE_PTR bpIV, const CK_ULONG lIVSize,
                                    uint64_t *lPlaintextSize,
                                    const CK_MECHANISM_TYPE decMechType)
{
    preCheck();

    CK_MECHANISM decMech = { decMechType, bpIV, lIVSize };
    CK_RV rv = pFunctionList->C_DecryptInit(hSession, &decMech, hKey);
    if(rv != CKR_OK)
        throw new TException("Can't initialize decrypt operation", (Error)rv);

    CK_ULONG lfPlaintextSize = 0;
    rv = pFunctionList->C_Decrypt(hSession, bpCiphertext, lCiphertextSize, NULL_PTR, &lfPlaintextSize);
    if(rv != CKR_OK)
        throw new TException("Can't get plaintext size", (Error)rv);

    CK_BYTE_PTR bpPlaintext = new CK_BYTE[lfPlaintextSize];
    memset(bpPlaintext, 0x00, lfPlaintextSize);

    rv = pFunctionList->C_Decrypt(hSession, bpCiphertext, lCiphertextSize, bpPlaintext, &lfPlaintextSize);
    if(rv != CKR_OK)
    {
        delete[] bpPlaintext;
        throw new TException("Can't decrypt ciphertext", (Error)rv);
    }

    *lPlaintextSize = (uint64_t)lfPlaintextSize;
    lfPlaintextSize = 0x00;

    return bpPlaintext;
}

byte_array pkcs11_core::crypto::TCryptoManager::sDecrypt(const byte_array &keyID, const byte_array &ciphertext, const byte_array *IV, const CK_MECHANISM_TYPE decMechType, const CK_OBJECT_CLASS keyClass)
{
    preCheck();

    std::vector<CK_OBJECT_HANDLE> keysHaldles = keyManager->getKeyHandle(keyID, keyClass);
    if(keysHaldles.size() == 0)
        throw new TException("Can't find key with such id", Error::KEY_HANDLE_HOT_FOUND);

    uint64_t lPlaintextSize = 0;
    CK_BYTE_PTR bpPlaintext = nullptr;
    if(IV != nullptr) bpPlaintext = decrypt(keysHaldles[0], (CK_BYTE_PTR)ciphertext.data(), (CK_ULONG)ciphertext.size(), (CK_BYTE_PTR)IV->data(), (CK_ULONG)IV->size(), &lPlaintextSize, decMechType);
    else bpPlaintext = decrypt(keysHaldles[0], (CK_BYTE_PTR)ciphertext.data(), (CK_ULONG)ciphertext.size(), NULL_PTR, 0, &lPlaintextSize, decMechType);
    byte_array result((byte*)bpPlaintext, (byte*)bpPlaintext + lPlaintextSize);
    memset(bpPlaintext, 0x00, lPlaintextSize);
    delete[] bpPlaintext;
    return result;
}

byte_array pkcs11_core::crypto::TCryptoManager::MAC_Gost28147_SIGN(const byte_array &keyID, const byte_array &plaintext, const byte_array &IV)
{
    return sMac(keyID, plaintext, &IV, 4, CKM_GOST28147_MAC, CKO_SECRET_KEY);
}

bool pkcs11_core::crypto::TCryptoManager::MAC_Gost28147_VERIFY(const byte_array &keyID, const byte_array &plaintext, const byte_array &IV, const byte_array &signature)
{
    return isSignatureCorrect(keyID, plaintext, &IV, signature, CKM_GOST28147_MAC, CKO_SECRET_KEY);
}

CK_BYTE_PTR pkcs11_core::crypto::TCryptoManager::mac(const CK_OBJECT_HANDLE hKey,
                                const CK_BYTE_PTR bpPlaintext, const CK_ULONG lPlaintextSize,
                                const CK_BYTE_PTR bpIV, const CK_ULONG lIVSize,
                                const uint64_t lMacSize,
                                const CK_MECHANISM_TYPE macMechType)
{
    preCheck();

    CK_MECHANISM macMech = { macMechType, bpIV, lIVSize };
    CK_RV rv = pFunctionList->C_SignInit(hSession, &macMech, hKey);
    if(rv != CKR_OK)
        throw new TException("Can't initialize sing operation", (Error)rv);

    CK_ULONG ulMacSize = lMacSize;
    CK_BYTE_PTR bpMac = new CK_BYTE[ulMacSize];
    memset(bpMac, 0x00, ulMacSize);
    rv = pFunctionList->C_Sign(hSession, bpPlaintext, lPlaintextSize, bpMac, &ulMacSize);
    if(rv != CKR_OK)
    {
        delete[] bpMac;
        throw new TException("Can't sign plaintext", (Error)rv);
    }

    return bpMac;
}

byte_array pkcs11_core::crypto::TCryptoManager::sMac(const byte_array &keyID, const byte_array &plaintext, const byte_array *IV, const uint64_t lMacSize, const CK_MECHANISM_TYPE macMechType, const CK_OBJECT_CLASS keyClass)
{
    preCheck();

    std::vector<CK_OBJECT_HANDLE> keysHandles = keyManager->getKeyHandle(keyID, keyClass);
    if(keysHandles.size() == 0)
        throw new TException("Can't find key with such id", Error::KEY_HANDLE_HOT_FOUND);

    CK_BYTE_PTR bpMac = nullptr;
    if(IV != nullptr) bpMac = mac(keysHandles[0], (CK_BYTE_PTR)plaintext.data(), (CK_ULONG)plaintext.size(), (CK_BYTE_PTR)IV->data(), (CK_ULONG)IV->size(), lMacSize, macMechType);
    else bpMac = mac(keysHandles[0], (CK_BYTE_PTR)plaintext.data(), (CK_ULONG)plaintext.size(), NULL_PTR, 0, lMacSize, macMechType);

    byte_array result((byte*)bpMac, (byte*)bpMac + lMacSize);
    memset(bpMac, 0x00, lMacSize);
    delete[] bpMac;

    return result;
}

bool pkcs11_core::crypto::TCryptoManager::verify(const CK_OBJECT_HANDLE hKey, const CK_BYTE_PTR bpPlaintext, const CK_ULONG lPlaintextSize, const CK_BYTE_PTR bpIV, const CK_ULONG lIVSize, const CK_BYTE_PTR bpSignature, const CK_ULONG lSignatureSize, const CK_MECHANISM_TYPE verMechType)
{
    preCheck();

    CK_MECHANISM verMech = { verMechType, bpIV, lIVSize };
    CK_RV rv = pFunctionList->C_VerifyInit(hSession, &verMech, hKey);
    if(rv != CKR_OK)
        throw new TException("Can't initialize verify operation", (Error)rv);

    rv = pFunctionList->C_Verify(hSession, bpPlaintext, lPlaintextSize, bpSignature, lSignatureSize);
    if(rv != CKR_OK && rv != CKR_SIGNATURE_INVALID)
        throw new TException("Can't verify signature", (Error)rv);

    return rv == CKR_OK;
}

bool pkcs11_core::crypto::TCryptoManager::isSignatureCorrect(const byte_array &keyID, const byte_array &plaintext, const byte_array *IV, const byte_array &signature, const CK_MECHANISM_TYPE verMechType, const CK_OBJECT_CLASS keyClass)
{
    preCheck();

    std::vector<CK_OBJECT_HANDLE> keysHandles = keyManager->getKeyHandle(keyID, keyClass);
    if(keysHandles.size() == 0)
        throw new TException("Can't find key with such id", Error::KEY_HANDLE_HOT_FOUND);

    if(IV != nullptr)
        return verify(keysHandles[0], (CK_BYTE_PTR)plaintext.data(), (CK_ULONG)plaintext.size(), (CK_BYTE_PTR)IV->data(), (CK_ULONG)IV->size(), (CK_BYTE_PTR)signature.data(), (CK_ULONG)signature.size(), verMechType);
    else
        return verify(keysHandles[0], (CK_BYTE_PTR)plaintext.data(), (CK_ULONG)plaintext.size(), NULL_PTR, 0, (CK_BYTE_PTR)signature.data(), (CK_ULONG)signature.size(), verMechType);
}
