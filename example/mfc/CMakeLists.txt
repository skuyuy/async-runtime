set(CMAKE_MFC_FLAG 2)
add_executable(
        async-mfc
        WIN32
        AsyncUIWorker.h
        AsyncUIWorkerMFC.cpp
        AsyncUIWorkerMFC.h
        AsyncUIWorkerMFC.rc
        AsyncUIWorkerMFCDlg.cpp
        AsyncUIWorkerMFCDlg.h
        framework.h
        pch.cpp
        pch.h
        resource.h
        SynchronizationContext.cpp
        SynchronizationContext.h
        targetver.h
)

target_link_libraries(
        async-mfc
        PRIVATE
        async
)
target_compile_definitions(async-mfc PRIVATE _AFXDLL)
add_definitions(-DUNICODE -D_UNICODE)
#target_compile_options(async-mfc PRIVATE "$<$<C_COMPILER_ID:MSVC>:/utf-8>")
#target_compile_options(async-mfc PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
