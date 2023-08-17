#pragma once
#ifndef __WMI_QUERY_HPP__
#define __WMI_QUERY_HPP__

#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>
#include <stdexcept>

#pragma comment(lib, "wbemuuid.lib")


// https://learn.microsoft.com/zh-cn/windows/win32/wmisdk/example--getting-wmi-data-from-the-local-computer
namespace WMIQuery {

class ResultProperty {
    friend class ResultObject;
private:
    VARIANT _prop;
public:
    ResultProperty() {
        VariantInit(&(this->_prop));
    }
    ~ResultProperty() {
        VariantClear(&(this->_prop));
    }
    const VARIANT& data(void) {
        return this->_prop; 
    }
};

class ResultObject {
    friend class ResultEnumerator;
private:
    IWbemClassObject* _obj_ptr;
    ResultObject(IWbemClassObject* obj_ptr) {
        this->_obj_ptr = obj_ptr;
    }
public:
    ~ResultObject() {
        this->_obj_ptr->Release();
    }
    void get_property(const wchar_t* name, ResultProperty* buf) {
        this->_obj_ptr->Get(name, 0, &(buf->_prop), 0, 0);
    }
    void get_property(const wchar_t* name, ResultProperty& buf) {
        this->_obj_ptr->Get(name, 0, &(buf._prop), 0, 0);
    }
};

class ResultEnumerator {
    friend class Backend;
private:
    IEnumWbemClassObject* _enumerator_ptr;
    ResultEnumerator(IEnumWbemClassObject* enumerator_ptr) {
        this->_enumerator_ptr = enumerator_ptr;
    }
public:
    ~ResultEnumerator() {
        this->_enumerator_ptr->Release();
    }
    template <typename Op>
    void traverse(Op callback) {
        IWbemClassObject *obj_ptr;
        ULONG it_ret = 0;
        HRESULT hres;
        
        while (true) {
            obj_ptr = NULL;
            hres = this->_enumerator_ptr->Next(
                WBEM_INFINITE, 1, &obj_ptr, &it_ret);
            if (!it_ret) break;
            callback(ResultObject(obj_ptr));
        }
    }
};

class Backend {
private:
    bool _COM_inited;
    IWbemLocator* _locator_ptr;
    IWbemServices* _service_ptr;
public:
    Backend() {
        HRESULT hres;
        this->_COM_inited = false;
        this->_locator_ptr = NULL;
        this->_service_ptr = NULL;

        hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hres)) throw std::runtime_error("Failed to initialize COM library.");
        this->_COM_inited = true;

        hres =  CoInitializeSecurity(
            NULL, 
            -1,                          // COM authentication
            NULL,                        // Authentication services
            NULL,                        // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
            RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
            NULL,                        // Authentication info
            EOAC_NONE,                   // Additional capabilities 
            NULL                         // Reserved
        );              
        if (FAILED(hres)) throw std::runtime_error("Failed to initialize security.");

        hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, 
                                IID_IWbemLocator, (LPVOID *)&(this->_locator_ptr));
        if (FAILED(hres)) throw std::runtime_error("Failed to create IWbemLocator object.");

        OLECHAR wmi_namespace[] = L"ROOT\\CIMV2";
        hres = this->_locator_ptr->ConnectServer(
            wmi_namespace,           // Object path of WMI namespace
            NULL,                    // User name. NULL = current user
            NULL,                    // User password. NULL = current
            0,                       // Locale. NULL indicates current
            0,                       // Security flags.
            0,                       // Authority (for example, Kerberos)
            0,                       // Context object 
            &(this->_service_ptr)    // pointer to IWbemServices proxy
        );
        if (FAILED(hres)) throw std::runtime_error("Could not connect.");

        hres = CoSetProxyBlanket(
            this->_service_ptr,          // Indicates the proxy to set
            RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
            RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
            NULL,                        // Server principal name 
            RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
            RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
            NULL,                        // client identity
            EOAC_NONE                    // proxy capabilities 
        );
        if (FAILED(hres)) throw std::runtime_error("Could not set proxy blanket.");
    }
    ~Backend() {
        if (this->_service_ptr) this->_service_ptr->Release();
        if (this->_locator_ptr) this->_locator_ptr->Release();
        if (this->_COM_inited) CoUninitialize();
    }
    ResultEnumerator wql_query(const wchar_t* for_what) {
        HRESULT hres;
        
        OLECHAR query_type[] = L"WQL";
        IEnumWbemClassObject* enumerator_ptr = NULL;
        hres = this->_service_ptr->ExecQuery(
            query_type, (BSTR)for_what,
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
            NULL, &enumerator_ptr
        );
        if (FAILED(hres)) throw std::runtime_error("Query failed.");

        return ResultEnumerator(enumerator_ptr);
    }
    ResultEnumerator wql_query(const std::wstring& for_what) {
        return this->wql_query(for_what.c_str());
    }
};

} // namespace WMIQuery

#endif // __WMI_QUERY_HPP__