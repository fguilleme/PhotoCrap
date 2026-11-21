!define PCR_CLASSES "Software\Classes"
!define PCR_TYPE "PhotoCrap.Project"
!define PCR_PROVIDER "{DA388CAD-0A58-4C18-BE98-35C2AFA18B01}"
!define PCR_THUMBNAIL "shellex\{E357FCCD-A995-4576-B01F-234630154E96}"

!macro RegisterPhotoCrapProjects
  SetRegView 64
  WriteRegStr SHCTX "${PCR_CLASSES}\${PCR_TYPE}" "" "PhotoCrap Project"
  WriteRegStr SHCTX "${PCR_CLASSES}\${PCR_TYPE}\DefaultIcon" "" '"$INSTDIR\bin\PhotoCrap.exe",0'
  WriteRegStr SHCTX "${PCR_CLASSES}\${PCR_TYPE}\shell\open\command" "" '"$INSTDIR\bin\PhotoCrap.exe" "%1"'
  WriteRegStr SHCTX "${PCR_CLASSES}\.pcr\OpenWithProgids" "${PCR_TYPE}" ""
  ReadRegStr $0 SHCTX "${PCR_CLASSES}\.pcr" ""
  StrCmp $0 "" 0 +2
    WriteRegStr SHCTX "${PCR_CLASSES}\.pcr" "" "${PCR_TYPE}"
  WriteRegStr SHCTX "${PCR_CLASSES}\CLSID\${PCR_PROVIDER}\InprocServer32" "" "$INSTDIR\bin\PhotoCrapThumbnail.dll"
  WriteRegStr SHCTX "${PCR_CLASSES}\CLSID\${PCR_PROVIDER}\InprocServer32" "ThreadingModel" "Apartment"
  ReadRegStr $0 SHCTX "${PCR_CLASSES}\.pcr\${PCR_THUMBNAIL}" ""
  StrCmp $0 "" 0 +2
    WriteRegStr SHCTX "${PCR_CLASSES}\.pcr\${PCR_THUMBNAIL}" "" "${PCR_PROVIDER}"
  SetRegView 32
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
!macroend

!macro UnregisterPhotoCrapProjects
  SetRegView 64
  ReadRegStr $0 SHCTX "${PCR_CLASSES}\.pcr" ""
  StrCmp $0 "${PCR_TYPE}" 0 +2
    DeleteRegValue SHCTX "${PCR_CLASSES}\.pcr" ""
  DeleteRegValue SHCTX "${PCR_CLASSES}\.pcr\OpenWithProgids" "${PCR_TYPE}"
  DeleteRegKey /ifempty SHCTX "${PCR_CLASSES}\.pcr\OpenWithProgids"
  ReadRegStr $0 SHCTX "${PCR_CLASSES}\.pcr\${PCR_THUMBNAIL}" ""
  StrCmp $0 "${PCR_PROVIDER}" 0 +2
    DeleteRegValue SHCTX "${PCR_CLASSES}\.pcr\${PCR_THUMBNAIL}" ""
  DeleteRegKey /ifempty SHCTX "${PCR_CLASSES}\.pcr\${PCR_THUMBNAIL}"
  DeleteRegKey /ifempty SHCTX "${PCR_CLASSES}\.pcr\shellex"
  DeleteRegKey /ifempty SHCTX "${PCR_CLASSES}\.pcr"
  DeleteRegKey SHCTX "${PCR_CLASSES}\${PCR_TYPE}"
  DeleteRegKey SHCTX "${PCR_CLASSES}\CLSID\${PCR_PROVIDER}"
  SetRegView 32
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
!macroend
