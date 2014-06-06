#ifndef _ACTIVEXML_H
#define _ACTIVEXML_H

#include <cviauto.h>

#ifdef __cplusplus
    extern "C" {
#endif
/* NICDBLD_BEGIN> Type Library Specific Types */

enum ActiveXMLEnum_tagDOMNodeType
{
	ActiveXMLConst_NODE_INVALID = 0,
	ActiveXMLConst_NODE_ELEMENT = 1,
	ActiveXMLConst_NODE_ATTRIBUTE = 2,
	ActiveXMLConst_NODE_TEXT = 3,
	ActiveXMLConst_NODE_CDATA_SECTION = 4,
	ActiveXMLConst_NODE_ENTITY_REFERENCE = 5,
	ActiveXMLConst_NODE_ENTITY = 6,
	ActiveXMLConst_NODE_PROCESSING_INSTRUCTION = 7,
	ActiveXMLConst_NODE_COMMENT = 8,
	ActiveXMLConst_NODE_DOCUMENT = 9,
	ActiveXMLConst_NODE_DOCUMENT_TYPE = 10,
	ActiveXMLConst_NODE_DOCUMENT_FRAGMENT = 11,
	ActiveXMLConst_NODE_NOTATION = 12,
	_ActiveXML_tagDOMNodeTypeForceSizeToFourBytes = 0xFFFFFFFF
};
enum ActiveXMLEnum__SERVERXMLHTTP_OPTION
{
	ActiveXMLConst_SXH_OPTION_URL = -1,
	ActiveXMLConst_SXH_OPTION_URL_CODEPAGE = 0,
	ActiveXMLConst_SXH_OPTION_ESCAPE_PERCENT_IN_URL = 1,
	ActiveXMLConst_SXH_OPTION_IGNORE_SERVER_SSL_CERT_ERROR_FLAGS = 2,
	ActiveXMLConst_SXH_OPTION_SELECT_CLIENT_SSL_CERT = 3,
	_ActiveXML__SERVERXMLHTTP_OPTIONForceSizeToFourBytes = 0xFFFFFFFF
};
enum ActiveXMLEnum__SXH_PROXY_SETTING
{
	ActiveXMLConst_SXH_PROXY_SET_DEFAULT = 0,
	ActiveXMLConst_SXH_PROXY_SET_PRECONFIG = 0,
	ActiveXMLConst_SXH_PROXY_SET_DIRECT = 1,
	ActiveXMLConst_SXH_PROXY_SET_PROXY = 2,
	_ActiveXML__SXH_PROXY_SETTINGForceSizeToFourBytes = 0xFFFFFFFF
};
enum ActiveXMLEnum__SOMITEMTYPE
{
	ActiveXMLConst_SOMITEM_SCHEMA = 4096,
	ActiveXMLConst_SOMITEM_ATTRIBUTE = 4097,
	ActiveXMLConst_SOMITEM_ATTRIBUTEGROUP = 4098,
	ActiveXMLConst_SOMITEM_NOTATION = 4099,
	ActiveXMLConst_SOMITEM_ANNOTATION = 4100,
	ActiveXMLConst_SOMITEM_IDENTITYCONSTRAINT = 4352,
	ActiveXMLConst_SOMITEM_KEY = 4353,
	ActiveXMLConst_SOMITEM_KEYREF = 4354,
	ActiveXMLConst_SOMITEM_UNIQUE = 4355,
	ActiveXMLConst_SOMITEM_ANYTYPE = 8192,
	ActiveXMLConst_SOMITEM_DATATYPE = 8448,
	ActiveXMLConst_SOMITEM_DATATYPE_ANYTYPE = 8449,
	ActiveXMLConst_SOMITEM_DATATYPE_ANYURI = 8450,
	ActiveXMLConst_SOMITEM_DATATYPE_BASE64BINARY = 8451,
	ActiveXMLConst_SOMITEM_DATATYPE_BOOLEAN = 8452,
	ActiveXMLConst_SOMITEM_DATATYPE_BYTE = 8453,
	ActiveXMLConst_SOMITEM_DATATYPE_DATE = 8454,
	ActiveXMLConst_SOMITEM_DATATYPE_DATETIME = 8455,
	ActiveXMLConst_SOMITEM_DATATYPE_DAY = 8456,
	ActiveXMLConst_SOMITEM_DATATYPE_DECIMAL = 8457,
	ActiveXMLConst_SOMITEM_DATATYPE_DOUBLE = 8458,
	ActiveXMLConst_SOMITEM_DATATYPE_DURATION = 8459,
	ActiveXMLConst_SOMITEM_DATATYPE_ENTITIES = 8460,
	ActiveXMLConst_SOMITEM_DATATYPE_ENTITY = 8461,
	ActiveXMLConst_SOMITEM_DATATYPE_FLOAT = 8462,
	ActiveXMLConst_SOMITEM_DATATYPE_HEXBINARY = 8463,
	ActiveXMLConst_SOMITEM_DATATYPE_ID = 8464,
	ActiveXMLConst_SOMITEM_DATATYPE_IDREF = 8465,
	ActiveXMLConst_SOMITEM_DATATYPE_IDREFS = 8466,
	ActiveXMLConst_SOMITEM_DATATYPE_INT = 8467,
	ActiveXMLConst_SOMITEM_DATATYPE_INTEGER = 8468,
	ActiveXMLConst_SOMITEM_DATATYPE_LANGUAGE = 8469,
	ActiveXMLConst_SOMITEM_DATATYPE_LONG = 8470,
	ActiveXMLConst_SOMITEM_DATATYPE_MONTH = 8471,
	ActiveXMLConst_SOMITEM_DATATYPE_MONTHDAY = 8472,
	ActiveXMLConst_SOMITEM_DATATYPE_NAME = 8473,
	ActiveXMLConst_SOMITEM_DATATYPE_NCNAME = 8474,
	ActiveXMLConst_SOMITEM_DATATYPE_NEGATIVEINTEGER = 8475,
	ActiveXMLConst_SOMITEM_DATATYPE_NMTOKEN = 8476,
	ActiveXMLConst_SOMITEM_DATATYPE_NMTOKENS = 8477,
	ActiveXMLConst_SOMITEM_DATATYPE_NONNEGATIVEINTEGER = 8478,
	ActiveXMLConst_SOMITEM_DATATYPE_NONPOSITIVEINTEGER = 8479,
	ActiveXMLConst_SOMITEM_DATATYPE_NORMALIZEDSTRING = 8480,
	ActiveXMLConst_SOMITEM_DATATYPE_NOTATION = 8481,
	ActiveXMLConst_SOMITEM_DATATYPE_POSITIVEINTEGER = 8482,
	ActiveXMLConst_SOMITEM_DATATYPE_QNAME = 8483,
	ActiveXMLConst_SOMITEM_DATATYPE_SHORT = 8484,
	ActiveXMLConst_SOMITEM_DATATYPE_STRING = 8485,
	ActiveXMLConst_SOMITEM_DATATYPE_TIME = 8486,
	ActiveXMLConst_SOMITEM_DATATYPE_TOKEN = 8487,
	ActiveXMLConst_SOMITEM_DATATYPE_UNSIGNEDBYTE = 8488,
	ActiveXMLConst_SOMITEM_DATATYPE_UNSIGNEDINT = 8489,
	ActiveXMLConst_SOMITEM_DATATYPE_UNSIGNEDLONG = 8490,
	ActiveXMLConst_SOMITEM_DATATYPE_UNSIGNEDSHORT = 8491,
	ActiveXMLConst_SOMITEM_DATATYPE_YEAR = 8492,
	ActiveXMLConst_SOMITEM_DATATYPE_YEARMONTH = 8493,
	ActiveXMLConst_SOMITEM_DATATYPE_ANYSIMPLETYPE = 8703,
	ActiveXMLConst_SOMITEM_SIMPLETYPE = 8704,
	ActiveXMLConst_SOMITEM_COMPLEXTYPE = 9216,
	ActiveXMLConst_SOMITEM_PARTICLE = 16384,
	ActiveXMLConst_SOMITEM_ANY = 16385,
	ActiveXMLConst_SOMITEM_ANYATTRIBUTE = 16386,
	ActiveXMLConst_SOMITEM_ELEMENT = 16387,
	ActiveXMLConst_SOMITEM_GROUP = 16640,
	ActiveXMLConst_SOMITEM_ALL = 16641,
	ActiveXMLConst_SOMITEM_CHOICE = 16642,
	ActiveXMLConst_SOMITEM_SEQUENCE = 16643,
	ActiveXMLConst_SOMITEM_EMPTYPARTICLE = 16644,
	ActiveXMLConst_SOMITEM_NULL = 2048,
	ActiveXMLConst_SOMITEM_NULL_TYPE = 10240,
	ActiveXMLConst_SOMITEM_NULL_ANY = 18433,
	ActiveXMLConst_SOMITEM_NULL_ANYATTRIBUTE = 18434,
	ActiveXMLConst_SOMITEM_NULL_ELEMENT = 18435,
	_ActiveXML__SOMITEMTYPEForceSizeToFourBytes = 0xFFFFFFFF
};
enum ActiveXMLEnum__SCHEMADERIVATIONMETHOD
{
	ActiveXMLConst_SCHEMADERIVATIONMETHOD_EMPTY = 0,
	ActiveXMLConst_SCHEMADERIVATIONMETHOD_SUBSTITUTION = 1,
	ActiveXMLConst_SCHEMADERIVATIONMETHOD_EXTENSION = 2,
	ActiveXMLConst_SCHEMADERIVATIONMETHOD_RESTRICTION = 4,
	ActiveXMLConst_SCHEMADERIVATIONMETHOD_LIST = 8,
	ActiveXMLConst_SCHEMADERIVATIONMETHOD_UNION = 16,
	ActiveXMLConst_SCHEMADERIVATIONMETHOD_ALL = 255,
	ActiveXMLConst_SCHEMADERIVATIONMETHOD_NONE = 256,
	_ActiveXML__SCHEMADERIVATIONMETHODForceSizeToFourBytes = 0xFFFFFFFF
};
enum ActiveXMLEnum__SCHEMATYPEVARIETY
{
	ActiveXMLConst_SCHEMATYPEVARIETY_NONE = -1,
	ActiveXMLConst_SCHEMATYPEVARIETY_ATOMIC = 0,
	ActiveXMLConst_SCHEMATYPEVARIETY_LIST = 1,
	ActiveXMLConst_SCHEMATYPEVARIETY_UNION = 2,
	_ActiveXML__SCHEMATYPEVARIETYForceSizeToFourBytes = 0xFFFFFFFF
};
enum ActiveXMLEnum__SCHEMAWHITESPACE
{
	ActiveXMLConst_SCHEMAWHITESPACE_NONE = -1,
	ActiveXMLConst_SCHEMAWHITESPACE_PRESERVE = 0,
	ActiveXMLConst_SCHEMAWHITESPACE_REPLACE = 1,
	ActiveXMLConst_SCHEMAWHITESPACE_COLLAPSE = 2,
	_ActiveXML__SCHEMAWHITESPACEForceSizeToFourBytes = 0xFFFFFFFF
};
enum ActiveXMLEnum__SCHEMACONTENTTYPE
{
	ActiveXMLConst_SCHEMACONTENTTYPE_EMPTY = 0,
	ActiveXMLConst_SCHEMACONTENTTYPE_TEXTONLY = 1,
	ActiveXMLConst_SCHEMACONTENTTYPE_ELEMENTONLY = 2,
	ActiveXMLConst_SCHEMACONTENTTYPE_MIXED = 3,
	_ActiveXML__SCHEMACONTENTTYPEForceSizeToFourBytes = 0xFFFFFFFF
};
enum ActiveXMLEnum__SCHEMAPROCESSCONTENTS
{
	ActiveXMLConst_SCHEMAPROCESSCONTENTS_NONE = 0,
	ActiveXMLConst_SCHEMAPROCESSCONTENTS_SKIP = 1,
	ActiveXMLConst_SCHEMAPROCESSCONTENTS_LAX = 2,
	ActiveXMLConst_SCHEMAPROCESSCONTENTS_STRICT = 3,
	_ActiveXML__SCHEMAPROCESSCONTENTSForceSizeToFourBytes = 0xFFFFFFFF
};
enum ActiveXMLEnum__SCHEMAUSE
{
	ActiveXMLConst_SCHEMAUSE_OPTIONAL = 0,
	ActiveXMLConst_SCHEMAUSE_PROHIBITED = 1,
	ActiveXMLConst_SCHEMAUSE_REQUIRED = 2,
	_ActiveXML__SCHEMAUSEForceSizeToFourBytes = 0xFFFFFFFF
};
enum ActiveXMLEnum__SXH_SERVER_CERT_OPTION
{
	ActiveXMLConst_SXH_SERVER_CERT_IGNORE_UNKNOWN_CA = 256,
	ActiveXMLConst_SXH_SERVER_CERT_IGNORE_WRONG_USAGE = 512,
	ActiveXMLConst_SXH_SERVER_CERT_IGNORE_CERT_CN_INVALID = 4096,
	ActiveXMLConst_SXH_SERVER_CERT_IGNORE_CERT_DATE_INVALID = 8192,
	ActiveXMLConst_SXH_SERVER_CERT_IGNORE_ALL_SERVER_ERRORS = 13056,
	_ActiveXML__SXH_SERVER_CERT_OPTIONForceSizeToFourBytes = 0xFFFFFFFF
};
typedef long ActiveXMLType_DOMNodeType;
typedef CAObjHandle ActiveXMLObj_IXMLDOMNode_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMNodeList_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMNamedNodeMap_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMDocument_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMDocumentType_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMImplementation_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMElement_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMDocumentFragment_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMText_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMComment_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMCDATASection_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMProcessingInstruction_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMAttribute_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMEntityReference_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMParseError_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMSchemaCollection;
typedef CAObjHandle ActiveXMLObj_ISchema;
typedef CAObjHandle ActiveXMLObj_ISchemaItem;
typedef CAObjHandle ActiveXMLObj_IXSLProcessor;
typedef long ActiveXMLType_SERVERXMLHTTP_OPTION;
typedef long ActiveXMLType_SXH_PROXY_SETTING;
typedef CAObjHandle ActiveXMLObj_IVBSAXEntityResolver;
typedef CAObjHandle ActiveXMLObj_IVBSAXContentHandler;
typedef CAObjHandle ActiveXMLObj_IVBSAXDTDHandler;
typedef CAObjHandle ActiveXMLObj_IVBSAXErrorHandler;
typedef CAObjHandle ActiveXMLObj_ISAXEntityResolver;
typedef CAObjHandle ActiveXMLObj_ISAXContentHandler;
typedef CAObjHandle ActiveXMLObj_ISAXDTDHandler;
typedef CAObjHandle ActiveXMLObj_ISAXErrorHandler;
typedef CAObjHandle ActiveXMLObj_ISAXLocator;
typedef CAObjHandle ActiveXMLObj_ISAXAttributes;
typedef CAObjHandle ActiveXMLObj_IVBSAXLocator;
typedef CAObjHandle ActiveXMLObj_IVBSAXAttributes;
typedef CAObjHandle ActiveXMLObj_IMXNamespacePrefixes;
typedef CAObjHandle ActiveXMLObj_IXMLDOMNode;
typedef CAObjHandle ActiveXMLObj_IXMLDOMParseErrorCollection_;
typedef CAObjHandle ActiveXMLObj_IXMLDOMParseError2_;
typedef CAObjHandle ActiveXMLObj_IXSLTemplate;
typedef CAObjHandle ActiveXMLObj_ISAXXMLReader;
typedef CAObjHandle ActiveXMLObj_IVBSAXXMLReader;
typedef CAObjHandle ActiveXMLObj_ISchemaElement;
typedef long ActiveXMLType_SOMITEMTYPE;
typedef CAObjHandle ActiveXMLObj_ISchemaType;
typedef CAObjHandle ActiveXMLObj_ISchemaComplexType;
typedef CAObjHandle ActiveXMLObj_ISchemaItemCollection;
typedef long ActiveXMLType_SCHEMADERIVATIONMETHOD;
typedef CAObjHandle ActiveXMLObj_ISchemaStringCollection;
typedef long ActiveXMLType_SCHEMATYPEVARIETY;
typedef long ActiveXMLType_SCHEMAWHITESPACE;
typedef CAObjHandle ActiveXMLObj_ISchemaAny;
typedef long ActiveXMLType_SCHEMACONTENTTYPE;
typedef CAObjHandle ActiveXMLObj_ISchemaModelGroup;
typedef long ActiveXMLType_SCHEMAPROCESSCONTENTS;
typedef long ActiveXMLType_SCHEMAUSE;
typedef CAObjHandle ActiveXMLObj_ISchemaIdentityConstraint;
typedef CAObjHandle ActiveXMLObj_IXMLDOMSelection_;
typedef long ActiveXMLType_SXH_SERVER_CERT_OPTION;
/* NICDBLD_END> Type Library Specific Types */

extern const IID ActiveXML_IID_IXMLDOMDocument2_;
extern const IID ActiveXML_IID_IXMLDOMDocument3_;
extern const IID ActiveXML_IID_IXMLDOMSchemaCollection;
extern const IID ActiveXML_IID_IXMLDOMSchemaCollection2;
extern const IID ActiveXML_IID_IXSLTemplate;
extern const IID ActiveXML_IID_IDSOControl;
extern const IID ActiveXML_IID_IXMLHTTPRequest;
extern const IID ActiveXML_IID_IServerXMLHTTPRequest;
extern const IID ActiveXML_IID_IServerXMLHTTPRequest2;
extern const IID ActiveXML_IID_IVBSAXXMLReader;
extern const IID ActiveXML_IID_ISAXXMLReader;
extern const IID ActiveXML_IID_IMXReaderControl;
extern const IID ActiveXML_IID_IMXWriter;
extern const IID ActiveXML_IID_ISAXContentHandler;
extern const IID ActiveXML_IID_ISAXErrorHandler;
extern const IID ActiveXML_IID_ISAXDTDHandler;
extern const IID ActiveXML_IID_ISAXLexicalHandler;
extern const IID ActiveXML_IID_ISAXDeclHandler;
extern const IID ActiveXML_IID_IVBSAXContentHandler;
extern const IID ActiveXML_IID_IVBSAXDeclHandler;
extern const IID ActiveXML_IID_IVBSAXDTDHandler;
extern const IID ActiveXML_IID_IVBSAXErrorHandler;
extern const IID ActiveXML_IID_IVBSAXLexicalHandler;
extern const IID ActiveXML_IID_IMXAttributes;
extern const IID ActiveXML_IID_IVBSAXAttributes;
extern const IID ActiveXML_IID_ISAXAttributes;
extern const IID ActiveXML_IID_IVBMXNamespaceManager;
extern const IID ActiveXML_IID_IMXNamespaceManager;
extern const IID ActiveXML_IID_XMLDOMDocumentEvents;
extern const IID ActiveXML_IID_IXMLDOMImplementation_;
extern const IID ActiveXML_IID_IXMLDOMNode_;
extern const IID ActiveXML_IID_IXMLDOMNodeList_;
extern const IID ActiveXML_IID_IXMLDOMNamedNodeMap_;
extern const IID ActiveXML_IID_IXMLDOMDocument_;
extern const IID ActiveXML_IID_IXMLDOMDocumentType_;
extern const IID ActiveXML_IID_IXMLDOMElement_;
extern const IID ActiveXML_IID_IXMLDOMAttribute_;
extern const IID ActiveXML_IID_IXMLDOMDocumentFragment_;
extern const IID ActiveXML_IID_IXMLDOMText_;
extern const IID ActiveXML_IID_IXMLDOMCharacterData_;
extern const IID ActiveXML_IID_IXMLDOMComment_;
extern const IID ActiveXML_IID_IXMLDOMCDATASection_;
extern const IID ActiveXML_IID_IXMLDOMProcessingInstruction_;
extern const IID ActiveXML_IID_IXMLDOMEntityReference_;
extern const IID ActiveXML_IID_IXMLDOMParseError_;
extern const IID ActiveXML_IID_IXMLDOMNotation_;
extern const IID ActiveXML_IID_IXMLDOMEntity_;
extern const IID ActiveXML_IID_IXMLDOMParseError2_;
extern const IID ActiveXML_IID_IXMLDOMParseErrorCollection_;
extern const IID ActiveXML_IID_IXTLRuntime;
extern const IID ActiveXML_IID_IXSLProcessor;
extern const IID ActiveXML_IID_ISAXEntityResolver;
extern const IID ActiveXML_IID_ISAXLocator;
extern const IID ActiveXML_IID_ISAXXMLFilter;
extern const IID ActiveXML_IID_IVBSAXEntityResolver;
extern const IID ActiveXML_IID_IVBSAXLocator;
extern const IID ActiveXML_IID_IVBSAXXMLFilter;
extern const IID ActiveXML_IID_IMXSchemaDeclHandler;
extern const IID ActiveXML_IID_ISchemaElement;
extern const IID ActiveXML_IID_ISchemaParticle;
extern const IID ActiveXML_IID_ISchemaItem;
extern const IID ActiveXML_IID_ISchema;
extern const IID ActiveXML_IID_ISchemaItemCollection;
extern const IID ActiveXML_IID_ISchemaStringCollection;
extern const IID ActiveXML_IID_ISchemaType;
extern const IID ActiveXML_IID_ISchemaComplexType;
extern const IID ActiveXML_IID_ISchemaAny;
extern const IID ActiveXML_IID_ISchemaModelGroup;
extern const IID ActiveXML_IID_IMXXMLFilter;
extern const IID ActiveXML_IID_ISchemaAttribute;
extern const IID ActiveXML_IID_ISchemaAttributeGroup;
extern const IID ActiveXML_IID_ISchemaIdentityConstraint;
extern const IID ActiveXML_IID_ISchemaNotation;
extern const IID ActiveXML_IID_IXMLDOMSelection_;
extern const IID ActiveXML_IID_IMXNamespacePrefixes;

HRESULT CVIFUNC ActiveXML_NewDOMDocumentIXMLDOMDocument2_ (const char *server,
                                                           int supportMultithreading,
                                                           LCID locale,
                                                           int reserved,
                                                           CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_OpenDOMDocumentIXMLDOMDocument2_ (const char *fileName,
                                                            const char *server,
                                                            int supportMultithreading,
                                                            LCID locale,
                                                            int reserved,
                                                            CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_ActiveDOMDocumentIXMLDOMDocument2_ (const char *server,
                                                              int supportMultithreading,
                                                              LCID locale,
                                                              int reserved,
                                                              CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_NewDOMDocument26IXMLDOMDocument2_ (const char *server,
                                                             int supportMultithreading,
                                                             LCID locale,
                                                             int reserved,
                                                             CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_OpenDOMDocument26IXMLDOMDocument2_ (const char *fileName,
                                                              const char *server,
                                                              int supportMultithreading,
                                                              LCID locale,
                                                              int reserved,
                                                              CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_ActiveDOMDocument26IXMLDOMDocument2_ (const char *server,
                                                                int supportMultithreading,
                                                                LCID locale,
                                                                int reserved,
                                                                CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_NewDOMDocument30IXMLDOMDocument2_ (const char *server,
                                                             int supportMultithreading,
                                                             LCID locale,
                                                             int reserved,
                                                             CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_OpenDOMDocument30IXMLDOMDocument2_ (const char *fileName,
                                                              const char *server,
                                                              int supportMultithreading,
                                                              LCID locale,
                                                              int reserved,
                                                              CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_ActiveDOMDocument30IXMLDOMDocument2_ (const char *server,
                                                                int supportMultithreading,
                                                                LCID locale,
                                                                int reserved,
                                                                CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_NewDOMDocument40IXMLDOMDocument2_ (const char *server,
                                                             int supportMultithreading,
                                                             LCID locale,
                                                             int reserved,
                                                             CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_OpenDOMDocument40IXMLDOMDocument2_ (const char *fileName,
                                                              const char *server,
                                                              int supportMultithreading,
                                                              LCID locale,
                                                              int reserved,
                                                              CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_ActiveDOMDocument40IXMLDOMDocument2_ (const char *server,
                                                                int supportMultithreading,
                                                                LCID locale,
                                                                int reserved,
                                                                CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_NewFreeThreadedDOMDocumentIXMLDOMDocument2_ (const char *server,
                                                                       int supportMultithreading,
                                                                       LCID locale,
                                                                       int reserved,
                                                                       CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_OpenFreeThreadedDOMDocumentIXMLDOMDocument2_ (const char *fileName,
                                                                        const char *server,
                                                                        int supportMultithreading,
                                                                        LCID locale,
                                                                        int reserved,
                                                                        CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_ActiveFreeThreadedDOMDocumentIXMLDOMDocument2_ (const char *server,
                                                                          int supportMultithreading,
                                                                          LCID locale,
                                                                          int reserved,
                                                                          CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_NewFreeThreadedDOMDocument26IXMLDOMDocument2_ (const char *server,
                                                                         int supportMultithreading,
                                                                         LCID locale,
                                                                         int reserved,
                                                                         CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_OpenFreeThreadedDOMDocument26IXMLDOMDocument2_ (const char *fileName,
                                                                          const char *server,
                                                                          int supportMultithreading,
                                                                          LCID locale,
                                                                          int reserved,
                                                                          CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_ActiveFreeThreadedDOMDocument26IXMLDOMDocument2_ (const char *server,
                                                                            int supportMultithreading,
                                                                            LCID locale,
                                                                            int reserved,
                                                                            CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_NewFreeThreadedDOMDocument30IXMLDOMDocument2_ (const char *server,
                                                                         int supportMultithreading,
                                                                         LCID locale,
                                                                         int reserved,
                                                                         CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_OpenFreeThreadedDOMDocument30IXMLDOMDocument2_ (const char *fileName,
                                                                          const char *server,
                                                                          int supportMultithreading,
                                                                          LCID locale,
                                                                          int reserved,
                                                                          CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_ActiveFreeThreadedDOMDocument30IXMLDOMDocument2_ (const char *server,
                                                                            int supportMultithreading,
                                                                            LCID locale,
                                                                            int reserved,
                                                                            CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_NewFreeThreadedDOMDocument40IXMLDOMDocument2_ (const char *server,
                                                                         int supportMultithreading,
                                                                         LCID locale,
                                                                         int reserved,
                                                                         CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_OpenFreeThreadedDOMDocument40IXMLDOMDocument2_ (const char *fileName,
                                                                          const char *server,
                                                                          int supportMultithreading,
                                                                          LCID locale,
                                                                          int reserved,
                                                                          CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_ActiveFreeThreadedDOMDocument40IXMLDOMDocument2_ (const char *server,
                                                                            int supportMultithreading,
                                                                            LCID locale,
                                                                            int reserved,
                                                                            CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetnodeName (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetnodeValue (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_SetnodeValue (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetnodeType (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetparentNode (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetchildNodes (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetfirstChild (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetlastChild (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetpreviousSibling (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetnextSibling (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Getattributes (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_insertBefore (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ newChild,
                                                         VARIANT refChild,
                                                         ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_replaceChild (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ newChild,
                                                         ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                         ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_removeChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ childNode,
                                                        ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_appendChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ newChild,
                                                        ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_hasChildNodes (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetownerDocument (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_cloneNode (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VBOOL deep,
                                                      ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetnodeTypeString (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Gettext (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Settext (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Getspecified (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Getdefinition (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetnodeTypedValue (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_SetnodeTypedValue (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetdataType (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_SetdataType (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Getxml (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_transformNode (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                          char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_selectNodes (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *queryString,
                                                        ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_selectSingleNode (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             const char *queryString,
                                                             ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Getparsed (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetnamespaceURI (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Getprefix (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetbaseName (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_transformNodeToObject (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                  VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Getdoctype (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMDocumentType_ *documentType);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Getimplementation (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMImplementation_ *impl);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetdocumentElement (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMElement_ *DOMElement);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_SetByRefdocumentElement (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    ActiveXMLObj_IXMLDOMElement_ DOMElement);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_createElement (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          const char *tagName,
                                                          ActiveXMLObj_IXMLDOMElement_ *element);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_createDocumentFragment (CAObjHandle objectHandle,
                                                                   ERRORINFO *errorInfo,
                                                                   ActiveXMLObj_IXMLDOMDocumentFragment_ *docFrag);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_createTextNode (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *data,
                                                           ActiveXMLObj_IXMLDOMText_ *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_createComment (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          const char *data,
                                                          ActiveXMLObj_IXMLDOMComment_ *comment);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_createCDATASection (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               const char *data,
                                                               ActiveXMLObj_IXMLDOMCDATASection_ *cdata);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_createProcessingInstruction (CAObjHandle objectHandle,
                                                                        ERRORINFO *errorInfo,
                                                                        const char *target,
                                                                        const char *data,
                                                                        ActiveXMLObj_IXMLDOMProcessingInstruction_ *pi);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_createAttribute (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            const char *name,
                                                            ActiveXMLObj_IXMLDOMAttribute_ *attribute);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_createEntityReference (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  const char *name,
                                                                  ActiveXMLObj_IXMLDOMEntityReference_ *entityRef);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_getElementsByTagName (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 const char *tagName,
                                                                 ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_createNode (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT type,
                                                       const char *name,
                                                       const char *namespaceURI,
                                                       ActiveXMLObj_IXMLDOMNode_ *node);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_nodeFromID (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       const char *idString,
                                                       ActiveXMLObj_IXMLDOMNode_ *node);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_load (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 VARIANT xmlSource,
                                                 VBOOL *isSuccessful);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetreadyState (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          long *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetparseError (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMParseError_ *errorObj);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Geturl (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **urlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Getasync (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VBOOL *isAsync);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Setasync (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VBOOL isAsync);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_abort (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_loadXML (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    const char *bstrXML,
                                                    VBOOL *isSuccessful);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_save (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 VARIANT destination);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetvalidateOnParse (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VBOOL *isValidating);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_SetvalidateOnParse (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VBOOL isValidating);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetresolveExternals (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                VBOOL *isResolving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_SetresolveExternals (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                VBOOL isResolving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_GetpreserveWhiteSpace (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  VBOOL *isPreserving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_SetpreserveWhiteSpace (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  VBOOL isPreserving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Setonreadystatechange (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  VARIANT newValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Setondataavailable (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VARIANT newValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Setontransformnode (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VARIANT newValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Getnamespaces (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMSchemaCollection *namespaceCollection);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_Getschemas (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT *otherCollection);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_SetByRefschemas (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VARIANT otherCollection);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_validate (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMParseError_ *errorObj);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_setProperty (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *name,
                                                        VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument2_getProperty (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *name,
                                                        VARIANT *value);

HRESULT CVIFUNC ActiveXML_NewDOMDocument60IXMLDOMDocument3_ (const char *server,
                                                             int supportMultithreading,
                                                             LCID locale,
                                                             int reserved,
                                                             CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_OpenDOMDocument60IXMLDOMDocument3_ (const char *fileName,
                                                              const char *server,
                                                              int supportMultithreading,
                                                              LCID locale,
                                                              int reserved,
                                                              CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_ActiveDOMDocument60IXMLDOMDocument3_ (const char *server,
                                                                int supportMultithreading,
                                                                LCID locale,
                                                                int reserved,
                                                                CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_NewFreeThreadedDOMDocument60IXMLDOMDocument3_ (const char *server,
                                                                         int supportMultithreading,
                                                                         LCID locale,
                                                                         int reserved,
                                                                         CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_OpenFreeThreadedDOMDocument60IXMLDOMDocument3_ (const char *fileName,
                                                                          const char *server,
                                                                          int supportMultithreading,
                                                                          LCID locale,
                                                                          int reserved,
                                                                          CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_ActiveFreeThreadedDOMDocument60IXMLDOMDocument3_ (const char *server,
                                                                            int supportMultithreading,
                                                                            LCID locale,
                                                                            int reserved,
                                                                            CAObjHandle *objectHandle);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetnodeName (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetnodeValue (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_SetnodeValue (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetnodeType (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetparentNode (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetchildNodes (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetfirstChild (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetlastChild (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetpreviousSibling (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetnextSibling (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Getattributes (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_insertBefore (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ newChild,
                                                         VARIANT refChild,
                                                         ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_replaceChild (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ newChild,
                                                         ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                         ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_removeChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ childNode,
                                                        ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_appendChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ newChild,
                                                        ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_hasChildNodes (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetownerDocument (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_cloneNode (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VBOOL deep,
                                                      ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetnodeTypeString (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Gettext (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Settext (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Getspecified (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Getdefinition (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetnodeTypedValue (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_SetnodeTypedValue (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetdataType (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_SetdataType (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Getxml (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_transformNode (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                          char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_selectNodes (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *queryString,
                                                        ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_selectSingleNode (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             const char *queryString,
                                                             ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Getparsed (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetnamespaceURI (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Getprefix (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetbaseName (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_transformNodeToObject (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                  VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Getdoctype (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMDocumentType_ *documentType);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Getimplementation (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMImplementation_ *impl);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetdocumentElement (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMElement_ *DOMElement);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_SetByRefdocumentElement (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    ActiveXMLObj_IXMLDOMElement_ DOMElement);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_createElement (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          const char *tagName,
                                                          ActiveXMLObj_IXMLDOMElement_ *element);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_createDocumentFragment (CAObjHandle objectHandle,
                                                                   ERRORINFO *errorInfo,
                                                                   ActiveXMLObj_IXMLDOMDocumentFragment_ *docFrag);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_createTextNode (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *data,
                                                           ActiveXMLObj_IXMLDOMText_ *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_createComment (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          const char *data,
                                                          ActiveXMLObj_IXMLDOMComment_ *comment);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_createCDATASection (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               const char *data,
                                                               ActiveXMLObj_IXMLDOMCDATASection_ *cdata);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_createProcessingInstruction (CAObjHandle objectHandle,
                                                                        ERRORINFO *errorInfo,
                                                                        const char *target,
                                                                        const char *data,
                                                                        ActiveXMLObj_IXMLDOMProcessingInstruction_ *pi);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_createAttribute (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            const char *name,
                                                            ActiveXMLObj_IXMLDOMAttribute_ *attribute);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_createEntityReference (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  const char *name,
                                                                  ActiveXMLObj_IXMLDOMEntityReference_ *entityRef);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_getElementsByTagName (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 const char *tagName,
                                                                 ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_createNode (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT type,
                                                       const char *name,
                                                       const char *namespaceURI,
                                                       ActiveXMLObj_IXMLDOMNode_ *node);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_nodeFromID (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       const char *idString,
                                                       ActiveXMLObj_IXMLDOMNode_ *node);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_load (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 VARIANT xmlSource,
                                                 VBOOL *isSuccessful);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetreadyState (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          long *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetparseError (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMParseError_ *errorObj);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Geturl (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **urlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Getasync (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VBOOL *isAsync);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Setasync (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VBOOL isAsync);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_abort (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_loadXML (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    const char *bstrXML,
                                                    VBOOL *isSuccessful);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_save (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 VARIANT destination);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetvalidateOnParse (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VBOOL *isValidating);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_SetvalidateOnParse (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VBOOL isValidating);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetresolveExternals (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                VBOOL *isResolving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_SetresolveExternals (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                VBOOL isResolving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_GetpreserveWhiteSpace (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  VBOOL *isPreserving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_SetpreserveWhiteSpace (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  VBOOL isPreserving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Setonreadystatechange (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  VARIANT newValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Setondataavailable (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VARIANT newValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Setontransformnode (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VARIANT newValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Getnamespaces (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMSchemaCollection *namespaceCollection);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_Getschemas (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT *otherCollection);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_SetByRefschemas (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VARIANT otherCollection);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_validate (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMParseError_ *errorObj);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_setProperty (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *name,
                                                        VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_getProperty (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *name,
                                                        VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_validateNode (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ node,
                                                         ActiveXMLObj_IXMLDOMParseError_ *errorObj);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument3_importNode (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ node,
                                                       VBOOL deep,
                                                       ActiveXMLObj_IXMLDOMNode_ *clone);

HRESULT CVIFUNC ActiveXML_IXMLDOMImplementation_hasFeature (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            const char *feature,
                                                            const char *version,
                                                            VBOOL *hasFeature);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetnodeName (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetnodeValue (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_SetnodeValue (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetnodeType (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetparentNode (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetchildNodes (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetfirstChild (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetlastChild (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetpreviousSibling (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetnextSibling (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_Getattributes (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_insertBefore (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    ActiveXMLObj_IXMLDOMNode_ newChild,
                                                    VARIANT refChild,
                                                    ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_replaceChild (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    ActiveXMLObj_IXMLDOMNode_ newChild,
                                                    ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                    ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_removeChild (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   ActiveXMLObj_IXMLDOMNode_ childNode,
                                                   ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_appendChild (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   ActiveXMLObj_IXMLDOMNode_ newChild,
                                                   ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_hasChildNodes (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetownerDocument (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_cloneNode (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 VBOOL deep,
                                                 ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetnodeTypeString (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_Gettext (CAObjHandle objectHandle,
                                               ERRORINFO *errorInfo, char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_Settext (CAObjHandle objectHandle,
                                               ERRORINFO *errorInfo,
                                               const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_Getspecified (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_Getdefinition (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetnodeTypedValue (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_SetnodeTypedValue (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetdataType (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_SetdataType (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_Getxml (CAObjHandle objectHandle,
                                              ERRORINFO *errorInfo,
                                              char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_transformNode (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                     char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_selectNodes (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   const char *queryString,
                                                   ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_selectSingleNode (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *queryString,
                                                        ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_Getparsed (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetnamespaceURI (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_Getprefix (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_GetbaseName (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMNode_transformNodeToObject (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                             VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMNodeList_Getitem (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   long index,
                                                   ActiveXMLObj_IXMLDOMNode_ *listItem);

HRESULT CVIFUNC ActiveXML_IXMLDOMNodeList_Getlength (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     long *listLength);

HRESULT CVIFUNC ActiveXML_IXMLDOMNodeList_nextNode (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    ActiveXMLObj_IXMLDOMNode_ *nextItem);

HRESULT CVIFUNC ActiveXML_IXMLDOMNodeList_reset (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo);

HRESULT CVIFUNC ActiveXML_IXMLDOMNodeList_Get_newEnum (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       LPUNKNOWN *ppUnk);

HRESULT CVIFUNC ActiveXML_IXMLDOMNamedNodeMap_getNamedItem (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            const char *name,
                                                            ActiveXMLObj_IXMLDOMNode_ *namedItem);

HRESULT CVIFUNC ActiveXML_IXMLDOMNamedNodeMap_setNamedItem (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNode_ newItem,
                                                            ActiveXMLObj_IXMLDOMNode_ *nameItem);

HRESULT CVIFUNC ActiveXML_IXMLDOMNamedNodeMap_removeNamedItem (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               const char *name,
                                                               ActiveXMLObj_IXMLDOMNode_ *namedItem);

HRESULT CVIFUNC ActiveXML_IXMLDOMNamedNodeMap_Getitem (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       long index,
                                                       ActiveXMLObj_IXMLDOMNode_ *listItem);

HRESULT CVIFUNC ActiveXML_IXMLDOMNamedNodeMap_Getlength (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         long *listLength);

HRESULT CVIFUNC ActiveXML_IXMLDOMNamedNodeMap_getQualifiedItem (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                const char *baseName,
                                                                const char *namespaceURI,
                                                                ActiveXMLObj_IXMLDOMNode_ *qualifiedItem);

HRESULT CVIFUNC ActiveXML_IXMLDOMNamedNodeMap_removeQualifiedItem (CAObjHandle objectHandle,
                                                                   ERRORINFO *errorInfo,
                                                                   const char *baseName,
                                                                   const char *namespaceURI,
                                                                   ActiveXMLObj_IXMLDOMNode_ *qualifiedItem);

HRESULT CVIFUNC ActiveXML_IXMLDOMNamedNodeMap_nextNode (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ *nextItem);

HRESULT CVIFUNC ActiveXML_IXMLDOMNamedNodeMap_reset (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo);

HRESULT CVIFUNC ActiveXML_IXMLDOMNamedNodeMap_Get_newEnum (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           LPUNKNOWN *ppUnk);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetnodeName (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetnodeValue (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_SetnodeValue (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetnodeType (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetparentNode (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetchildNodes (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetfirstChild (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetlastChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetpreviousSibling (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetnextSibling (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Getattributes (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_insertBefore (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ newChild,
                                                        VARIANT refChild,
                                                        ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_replaceChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ newChild,
                                                        ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                        ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_removeChild (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ childNode,
                                                       ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_appendChild (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ newChild,
                                                       ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_hasChildNodes (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetownerDocument (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_cloneNode (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VBOOL deep,
                                                     ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetnodeTypeString (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Gettext (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Settext (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Getspecified (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Getdefinition (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetnodeTypedValue (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_SetnodeTypedValue (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetdataType (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_SetdataType (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Getxml (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_transformNode (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                         char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_selectNodes (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       const char *queryString,
                                                       ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_selectSingleNode (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            const char *queryString,
                                                            ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Getparsed (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetnamespaceURI (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Getprefix (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetbaseName (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_transformNodeToObject (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                 VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Getdoctype (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLObj_IXMLDOMDocumentType_ *documentType);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Getimplementation (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMImplementation_ *impl);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetdocumentElement (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMElement_ *DOMElement);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_SetByRefdocumentElement (CAObjHandle objectHandle,
                                                                   ERRORINFO *errorInfo,
                                                                   ActiveXMLObj_IXMLDOMElement_ DOMElement);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_createElement (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         const char *tagName,
                                                         ActiveXMLObj_IXMLDOMElement_ *element);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_createDocumentFragment (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  ActiveXMLObj_IXMLDOMDocumentFragment_ *docFrag);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_createTextNode (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          const char *data,
                                                          ActiveXMLObj_IXMLDOMText_ *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_createComment (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         const char *data,
                                                         ActiveXMLObj_IXMLDOMComment_ *comment);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_createCDATASection (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              const char *data,
                                                              ActiveXMLObj_IXMLDOMCDATASection_ *cdata);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_createProcessingInstruction (CAObjHandle objectHandle,
                                                                       ERRORINFO *errorInfo,
                                                                       const char *target,
                                                                       const char *data,
                                                                       ActiveXMLObj_IXMLDOMProcessingInstruction_ *pi);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_createAttribute (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *name,
                                                           ActiveXMLObj_IXMLDOMAttribute_ *attribute);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_createEntityReference (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 const char *name,
                                                                 ActiveXMLObj_IXMLDOMEntityReference_ *entityRef);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_getElementsByTagName (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                const char *tagName,
                                                                ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_createNode (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VARIANT type,
                                                      const char *name,
                                                      const char *namespaceURI,
                                                      ActiveXMLObj_IXMLDOMNode_ *node);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_nodeFromID (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      const char *idString,
                                                      ActiveXMLObj_IXMLDOMNode_ *node);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_load (CAObjHandle objectHandle,
                                                ERRORINFO *errorInfo,
                                                VARIANT xmlSource,
                                                VBOOL *isSuccessful);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetreadyState (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         long *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetparseError (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMParseError_ *errorObj);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Geturl (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  char **urlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Getasync (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VBOOL *isAsync);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Setasync (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VBOOL isAsync);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_abort (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_loadXML (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   const char *bstrXML,
                                                   VBOOL *isSuccessful);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_save (CAObjHandle objectHandle,
                                                ERRORINFO *errorInfo,
                                                VARIANT destination);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetvalidateOnParse (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VBOOL *isValidating);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_SetvalidateOnParse (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VBOOL isValidating);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetresolveExternals (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VBOOL *isResolving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_SetresolveExternals (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VBOOL isResolving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_GetpreserveWhiteSpace (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 VBOOL *isPreserving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_SetpreserveWhiteSpace (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 VBOOL isPreserving);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Setonreadystatechange (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 VARIANT newValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Setondataavailable (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VARIANT newValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocument_Setontransformnode (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VARIANT newValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetnodeName (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetnodeValue (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_SetnodeValue (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetnodeType (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetparentNode (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetchildNodes (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetfirstChild (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetlastChild (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetpreviousSibling (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetnextSibling (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_Getattributes (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_insertBefore (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNode_ newChild,
                                                            VARIANT refChild,
                                                            ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_replaceChild (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNode_ newChild,
                                                            ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                            ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_removeChild (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMNode_ childNode,
                                                           ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_appendChild (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMNode_ newChild,
                                                           ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_hasChildNodes (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetownerDocument (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_cloneNode (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VBOOL deep,
                                                         ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetnodeTypeString (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_Gettext (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_Settext (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_Getspecified (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_Getdefinition (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetnodeTypedValue (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_SetnodeTypedValue (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetdataType (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_SetdataType (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_Getxml (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_transformNode (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                             char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_selectNodes (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *queryString,
                                                           ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_selectSingleNode (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                const char *queryString,
                                                                ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_Getparsed (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetnamespaceURI (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_Getprefix (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_GetbaseName (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_transformNodeToObject (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                     VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_Getname (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **rootName);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_Getentities (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMNamedNodeMap_ *entityMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentType_Getnotations (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNamedNodeMap_ *notationMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetnodeName (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetnodeValue (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_SetnodeValue (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetnodeType (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetparentNode (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetchildNodes (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetfirstChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetlastChild (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetpreviousSibling (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetnextSibling (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_Getattributes (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_insertBefore (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ newChild,
                                                       VARIANT refChild,
                                                       ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_replaceChild (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ newChild,
                                                       ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                       ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_removeChild (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLObj_IXMLDOMNode_ childNode,
                                                      ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_appendChild (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLObj_IXMLDOMNode_ newChild,
                                                      ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_hasChildNodes (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetownerDocument (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_cloneNode (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VBOOL deep,
                                                    ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetnodeTypeString (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_Gettext (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_Settext (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_Getspecified (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_Getdefinition (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetnodeTypedValue (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_SetnodeTypedValue (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetdataType (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_SetdataType (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_Getxml (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_transformNode (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                        char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_selectNodes (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      const char *queryString,
                                                      ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_selectSingleNode (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *queryString,
                                                           ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_Getparsed (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetnamespaceURI (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_Getprefix (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GetbaseName (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_transformNodeToObject (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_GettagName (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     char **tagName);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_getAttribute (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       const char *name,
                                                       VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_setAttribute (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       const char *name,
                                                       VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_removeAttribute (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          const char *name);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_getAttributeNode (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *name,
                                                           ActiveXMLObj_IXMLDOMAttribute_ *attributeNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_setAttributeNode (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMAttribute_ DOMAttribute,
                                                           ActiveXMLObj_IXMLDOMAttribute_ *attributeNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_removeAttributeNode (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMAttribute_ DOMAttribute,
                                                              ActiveXMLObj_IXMLDOMAttribute_ *attributeNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_getElementsByTagName (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               const char *tagName,
                                                               ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMElement_normalize (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetnodeName (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetnodeValue (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_SetnodeValue (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetnodeType (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetparentNode (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetchildNodes (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetfirstChild (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetlastChild (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetpreviousSibling (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetnextSibling (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_Getattributes (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_insertBefore (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ newChild,
                                                         VARIANT refChild,
                                                         ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_replaceChild (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ newChild,
                                                         ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                         ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_removeChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ childNode,
                                                        ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_appendChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ newChild,
                                                        ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_hasChildNodes (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetownerDocument (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_cloneNode (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VBOOL deep,
                                                      ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetnodeTypeString (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_Gettext (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_Settext (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_Getspecified (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_Getdefinition (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetnodeTypedValue (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_SetnodeTypedValue (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetdataType (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_SetdataType (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_Getxml (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_transformNode (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                          char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_selectNodes (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *queryString,
                                                        ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_selectSingleNode (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             const char *queryString,
                                                             ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_Getparsed (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetnamespaceURI (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_Getprefix (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_GetbaseName (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_transformNodeToObject (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                  VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_Getname (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    char **attributeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_Getvalue (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VARIANT *attributeValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMAttribute_Setvalue (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VARIANT attributeValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetnodeName (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetnodeValue (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_SetnodeValue (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetnodeType (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetparentNode (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetchildNodes (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetfirstChild (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetlastChild (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetpreviousSibling (CAObjHandle objectHandle,
                                                                      ERRORINFO *errorInfo,
                                                                      ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetnextSibling (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_Getattributes (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_insertBefore (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMNode_ newChild,
                                                                VARIANT refChild,
                                                                ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_replaceChild (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMNode_ newChild,
                                                                ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                                ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_removeChild (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMNode_ childNode,
                                                               ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_appendChild (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMNode_ newChild,
                                                               ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_hasChildNodes (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetownerDocument (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_cloneNode (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             VBOOL deep,
                                                             ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetnodeTypeString (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_Gettext (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_Settext (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_Getspecified (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_Getdefinition (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetnodeTypedValue (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_SetnodeTypedValue (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetdataType (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_SetdataType (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_Getxml (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_transformNode (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                 char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_selectNodes (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               const char *queryString,
                                                               ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_selectSingleNode (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    const char *queryString,
                                                                    ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_Getparsed (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetnamespaceURI (CAObjHandle objectHandle,
                                                                   ERRORINFO *errorInfo,
                                                                   char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_Getprefix (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_GetbaseName (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMDocumentFragment_transformNodeToObject (CAObjHandle objectHandle,
                                                                         ERRORINFO *errorInfo,
                                                                         ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                         VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetnodeName (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetnodeValue (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_SetnodeValue (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetnodeType (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetparentNode (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetchildNodes (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetfirstChild (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetlastChild (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetpreviousSibling (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetnextSibling (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_Getattributes (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_insertBefore (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    ActiveXMLObj_IXMLDOMNode_ newChild,
                                                    VARIANT refChild,
                                                    ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_replaceChild (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    ActiveXMLObj_IXMLDOMNode_ newChild,
                                                    ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                    ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_removeChild (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   ActiveXMLObj_IXMLDOMNode_ childNode,
                                                   ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_appendChild (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   ActiveXMLObj_IXMLDOMNode_ newChild,
                                                   ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_hasChildNodes (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetownerDocument (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_cloneNode (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 VBOOL deep,
                                                 ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetnodeTypeString (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_Gettext (CAObjHandle objectHandle,
                                               ERRORINFO *errorInfo, char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_Settext (CAObjHandle objectHandle,
                                               ERRORINFO *errorInfo,
                                               const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_Getspecified (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_Getdefinition (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetnodeTypedValue (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_SetnodeTypedValue (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetdataType (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_SetdataType (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_Getxml (CAObjHandle objectHandle,
                                              ERRORINFO *errorInfo,
                                              char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_transformNode (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                     char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_selectNodes (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   const char *queryString,
                                                   ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_selectSingleNode (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *queryString,
                                                        ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_Getparsed (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetnamespaceURI (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_Getprefix (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_GetbaseName (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_transformNodeToObject (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                             VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_Getdata (CAObjHandle objectHandle,
                                               ERRORINFO *errorInfo, char **data);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_Setdata (CAObjHandle objectHandle,
                                               ERRORINFO *errorInfo,
                                               const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_Getlength (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 long *dataLength);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_substringData (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     long offset, long count,
                                                     char **data);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_appendData (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_insertData (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  long offset, const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_deleteData (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  long offset, long count);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_replaceData (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   long offset, long count,
                                                   const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMText_splitText (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 long offset,
                                                 ActiveXMLObj_IXMLDOMText_ *rightHandTextNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetnodeName (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetnodeValue (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_SetnodeValue (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetnodeType (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetparentNode (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetchildNodes (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetfirstChild (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetlastChild (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetpreviousSibling (CAObjHandle objectHandle,
                                                                   ERRORINFO *errorInfo,
                                                                   ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetnextSibling (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_Getattributes (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_insertBefore (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ newChild,
                                                             VARIANT refChild,
                                                             ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_replaceChild (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ newChild,
                                                             ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                             ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_removeChild (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNode_ childNode,
                                                            ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_appendChild (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNode_ newChild,
                                                            ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_hasChildNodes (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetownerDocument (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_cloneNode (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          VBOOL deep,
                                                          ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetnodeTypeString (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_Gettext (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_Settext (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_Getspecified (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_Getdefinition (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetnodeTypedValue (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_SetnodeTypedValue (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetdataType (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_SetdataType (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_Getxml (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_transformNode (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                              char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_selectNodes (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            const char *queryString,
                                                            ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_selectSingleNode (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 const char *queryString,
                                                                 ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_Getparsed (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetnamespaceURI (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_Getprefix (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_GetbaseName (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_transformNodeToObject (CAObjHandle objectHandle,
                                                                      ERRORINFO *errorInfo,
                                                                      ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                      VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_Getdata (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        char **data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_Setdata (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_Getlength (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          long *dataLength);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_substringData (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              long offset,
                                                              long count,
                                                              char **data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_appendData (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_insertData (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           long offset,
                                                           const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_deleteData (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           long offset,
                                                           long count);

HRESULT CVIFUNC ActiveXML_IXMLDOMCharacterData_replaceData (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            long offset,
                                                            long count,
                                                            const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetnodeName (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetnodeValue (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_SetnodeValue (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetnodeType (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetparentNode (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetchildNodes (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetfirstChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetlastChild (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetpreviousSibling (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetnextSibling (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_Getattributes (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_insertBefore (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ newChild,
                                                       VARIANT refChild,
                                                       ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_replaceChild (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ newChild,
                                                       ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                       ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_removeChild (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLObj_IXMLDOMNode_ childNode,
                                                      ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_appendChild (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLObj_IXMLDOMNode_ newChild,
                                                      ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_hasChildNodes (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetownerDocument (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_cloneNode (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VBOOL deep,
                                                    ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetnodeTypeString (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_Gettext (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_Settext (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_Getspecified (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_Getdefinition (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetnodeTypedValue (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_SetnodeTypedValue (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetdataType (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_SetdataType (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_Getxml (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_transformNode (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                        char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_selectNodes (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      const char *queryString,
                                                      ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_selectSingleNode (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *queryString,
                                                           ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_Getparsed (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetnamespaceURI (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_Getprefix (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_GetbaseName (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_transformNodeToObject (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_Getdata (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  char **data);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_Setdata (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_Getlength (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    long *dataLength);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_substringData (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        long offset, long count,
                                                        char **data);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_appendData (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_insertData (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     long offset,
                                                     const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_deleteData (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     long offset, long count);

HRESULT CVIFUNC ActiveXML_IXMLDOMComment_replaceData (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      long offset, long count,
                                                      const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetnodeName (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetnodeValue (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_SetnodeValue (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetnodeType (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetparentNode (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetchildNodes (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetfirstChild (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetlastChild (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetpreviousSibling (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetnextSibling (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_Getattributes (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_insertBefore (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNode_ newChild,
                                                            VARIANT refChild,
                                                            ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_replaceChild (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNode_ newChild,
                                                            ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                            ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_removeChild (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMNode_ childNode,
                                                           ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_appendChild (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMNode_ newChild,
                                                           ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_hasChildNodes (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetownerDocument (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_cloneNode (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VBOOL deep,
                                                         ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetnodeTypeString (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_Gettext (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_Settext (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_Getspecified (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_Getdefinition (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetnodeTypedValue (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_SetnodeTypedValue (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetdataType (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_SetdataType (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_Getxml (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_transformNode (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                             char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_selectNodes (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           const char *queryString,
                                                           ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_selectSingleNode (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                const char *queryString,
                                                                ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_Getparsed (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetnamespaceURI (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_Getprefix (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_GetbaseName (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_transformNodeToObject (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                     VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_Getdata (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_Setdata (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_Getlength (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         long *dataLength);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_substringData (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             long offset,
                                                             long count,
                                                             char **data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_appendData (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_insertData (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          long offset,
                                                          const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_deleteData (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          long offset,
                                                          long count);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_replaceData (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           long offset,
                                                           long count,
                                                           const char *data);

HRESULT CVIFUNC ActiveXML_IXMLDOMCDATASection_splitText (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         long offset,
                                                         ActiveXMLObj_IXMLDOMText_ *rightHandTextNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetnodeName (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetnodeValue (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_SetnodeValue (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetnodeType (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetparentNode (CAObjHandle objectHandle,
                                                                      ERRORINFO *errorInfo,
                                                                      ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetchildNodes (CAObjHandle objectHandle,
                                                                      ERRORINFO *errorInfo,
                                                                      ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetfirstChild (CAObjHandle objectHandle,
                                                                      ERRORINFO *errorInfo,
                                                                      ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetlastChild (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetpreviousSibling (CAObjHandle objectHandle,
                                                                           ERRORINFO *errorInfo,
                                                                           ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetnextSibling (CAObjHandle objectHandle,
                                                                       ERRORINFO *errorInfo,
                                                                       ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_Getattributes (CAObjHandle objectHandle,
                                                                      ERRORINFO *errorInfo,
                                                                      ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_insertBefore (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     ActiveXMLObj_IXMLDOMNode_ newChild,
                                                                     VARIANT refChild,
                                                                     ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_replaceChild (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     ActiveXMLObj_IXMLDOMNode_ newChild,
                                                                     ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                                     ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_removeChild (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    ActiveXMLObj_IXMLDOMNode_ childNode,
                                                                    ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_appendChild (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    ActiveXMLObj_IXMLDOMNode_ newChild,
                                                                    ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_hasChildNodes (CAObjHandle objectHandle,
                                                                      ERRORINFO *errorInfo,
                                                                      VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetownerDocument (CAObjHandle objectHandle,
                                                                         ERRORINFO *errorInfo,
                                                                         ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_cloneNode (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  VBOOL deep,
                                                                  ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetnodeTypeString (CAObjHandle objectHandle,
                                                                          ERRORINFO *errorInfo,
                                                                          char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_Gettext (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_Settext (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_Getspecified (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_Getdefinition (CAObjHandle objectHandle,
                                                                      ERRORINFO *errorInfo,
                                                                      ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetnodeTypedValue (CAObjHandle objectHandle,
                                                                          ERRORINFO *errorInfo,
                                                                          VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_SetnodeTypedValue (CAObjHandle objectHandle,
                                                                          ERRORINFO *errorInfo,
                                                                          VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetdataType (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_SetdataType (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_Getxml (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_transformNode (CAObjHandle objectHandle,
                                                                      ERRORINFO *errorInfo,
                                                                      ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                      char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_selectNodes (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    const char *queryString,
                                                                    ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_selectSingleNode (CAObjHandle objectHandle,
                                                                         ERRORINFO *errorInfo,
                                                                         const char *queryString,
                                                                         ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_Getparsed (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetnamespaceURI (CAObjHandle objectHandle,
                                                                        ERRORINFO *errorInfo,
                                                                        char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_Getprefix (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_GetbaseName (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_transformNodeToObject (CAObjHandle objectHandle,
                                                                              ERRORINFO *errorInfo,
                                                                              ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                              VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_Gettarget (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_Getdata (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                char **value);

HRESULT CVIFUNC ActiveXML_IXMLDOMProcessingInstruction_Setdata (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                const char *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetnodeName (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetnodeValue (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_SetnodeValue (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetnodeType (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetparentNode (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetchildNodes (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetfirstChild (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetlastChild (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetpreviousSibling (CAObjHandle objectHandle,
                                                                     ERRORINFO *errorInfo,
                                                                     ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetnextSibling (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_Getattributes (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_insertBefore (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMNode_ newChild,
                                                               VARIANT refChild,
                                                               ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_replaceChild (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMNode_ newChild,
                                                               ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                               ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_removeChild (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNode_ childNode,
                                                              ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_appendChild (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNode_ newChild,
                                                              ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_hasChildNodes (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetownerDocument (CAObjHandle objectHandle,
                                                                   ERRORINFO *errorInfo,
                                                                   ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_cloneNode (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VBOOL deep,
                                                            ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetnodeTypeString (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_Gettext (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_Settext (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_Getspecified (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_Getdefinition (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetnodeTypedValue (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_SetnodeTypedValue (CAObjHandle objectHandle,
                                                                    ERRORINFO *errorInfo,
                                                                    VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetdataType (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_SetdataType (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_Getxml (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_transformNode (CAObjHandle objectHandle,
                                                                ERRORINFO *errorInfo,
                                                                ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_selectNodes (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              const char *queryString,
                                                              ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_selectSingleNode (CAObjHandle objectHandle,
                                                                   ERRORINFO *errorInfo,
                                                                   const char *queryString,
                                                                   ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_Getparsed (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetnamespaceURI (CAObjHandle objectHandle,
                                                                  ERRORINFO *errorInfo,
                                                                  char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_Getprefix (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_GetbaseName (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntityReference_transformNodeToObject (CAObjHandle objectHandle,
                                                                        ERRORINFO *errorInfo,
                                                                        ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                        VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError_GeterrorCode (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          long *errorCode);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError_Geturl (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    char **urlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError_Getreason (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **reasonString);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError_GetsrcText (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        char **sourceString);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError_Getline (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     long *lineNumber);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError_Getlinepos (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        long *linePosition);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError_Getfilepos (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        long *filePosition);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetnodeName (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetnodeValue (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_SetnodeValue (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetnodeType (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetparentNode (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetchildNodes (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetfirstChild (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetlastChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetpreviousSibling (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetnextSibling (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_Getattributes (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_insertBefore (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ newChild,
                                                        VARIANT refChild,
                                                        ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_replaceChild (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ newChild,
                                                        ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                        ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_removeChild (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ childNode,
                                                       ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_appendChild (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ newChild,
                                                       ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_hasChildNodes (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetownerDocument (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_cloneNode (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VBOOL deep,
                                                     ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetnodeTypeString (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_Gettext (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_Settext (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_Getspecified (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_Getdefinition (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetnodeTypedValue (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_SetnodeTypedValue (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo,
                                                             VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetdataType (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_SetdataType (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_Getxml (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_transformNode (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                         char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_selectNodes (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       const char *queryString,
                                                       ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_selectSingleNode (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            const char *queryString,
                                                            ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_Getparsed (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetnamespaceURI (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_Getprefix (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetbaseName (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_transformNodeToObject (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                                 VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetpublicId (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT *publicId);

HRESULT CVIFUNC ActiveXML_IXMLDOMNotation_GetsystemId (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VARIANT *systemId);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetnodeName (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetnodeValue (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_SetnodeValue (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VARIANT value);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetnodeType (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLType_DOMNodeType *type);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetparentNode (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ *parent);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetchildNodes (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNodeList_ *childList);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetfirstChild (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ *firstChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetlastChild (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLObj_IXMLDOMNode_ *lastChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetpreviousSibling (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNode_ *previousSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetnextSibling (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        ActiveXMLObj_IXMLDOMNode_ *nextSibling);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_Getattributes (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNamedNodeMap_ *attributeMap);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_insertBefore (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLObj_IXMLDOMNode_ newChild,
                                                      VARIANT refChild,
                                                      ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_replaceChild (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      ActiveXMLObj_IXMLDOMNode_ newChild,
                                                      ActiveXMLObj_IXMLDOMNode_ oldChild,
                                                      ActiveXMLObj_IXMLDOMNode_ *outOldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_removeChild (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ childNode,
                                                     ActiveXMLObj_IXMLDOMNode_ *oldChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_appendChild (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ newChild,
                                                     ActiveXMLObj_IXMLDOMNode_ *outNewChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_hasChildNodes (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       VBOOL *hasChild);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetownerDocument (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          ActiveXMLObj_IXMLDOMDocument_ *XMLDOMDocument);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_cloneNode (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   VBOOL deep,
                                                   ActiveXMLObj_IXMLDOMNode_ *cloneRoot);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetnodeTypeString (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           char **nodeType);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_Gettext (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 char **text);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_Settext (CAObjHandle objectHandle,
                                                 ERRORINFO *errorInfo,
                                                 const char *text);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_Getspecified (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      VBOOL *isSpecified);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_Getdefinition (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ *definitionNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetnodeTypedValue (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           VARIANT *typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_SetnodeTypedValue (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           VARIANT typedValue);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetdataType (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VARIANT *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_SetdataType (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     const char *dataTypeName);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_Getxml (CAObjHandle objectHandle,
                                                ERRORINFO *errorInfo,
                                                char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_transformNode (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                       char **xmlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_selectNodes (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     const char *queryString,
                                                     ActiveXMLObj_IXMLDOMNodeList_ *resultList);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_selectSingleNode (CAObjHandle objectHandle,
                                                          ERRORINFO *errorInfo,
                                                          const char *queryString,
                                                          ActiveXMLObj_IXMLDOMNode_ *resultNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_Getparsed (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   VBOOL *isParsed);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetnamespaceURI (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         char **namespaceURI);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_Getprefix (CAObjHandle objectHandle,
                                                   ERRORINFO *errorInfo,
                                                   char **prefixString);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetbaseName (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     char **nameString);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_transformNodeToObject (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMNode_ stylesheet,
                                                               VARIANT outputObject);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetpublicId (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VARIANT *publicId);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetsystemId (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     VARIANT *systemId);

HRESULT CVIFUNC ActiveXML_IXMLDOMEntity_GetnotationName (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         char **name);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError2_GeterrorCode (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           long *errorCode);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError2_Geturl (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     char **urlString);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError2_Getreason (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        char **reasonString);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError2_GetsrcText (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         char **sourceString);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError2_Getline (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      long *lineNumber);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError2_Getlinepos (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         long *linePosition);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError2_Getfilepos (CAObjHandle objectHandle,
                                                         ERRORINFO *errorInfo,
                                                         long *filePosition);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError2_GeterrorXPath (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            char **xpathexpr);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError2_GetallErrors (CAObjHandle objectHandle,
                                                           ERRORINFO *errorInfo,
                                                           ActiveXMLObj_IXMLDOMParseErrorCollection_ *allErrors);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError2_errorParameters (CAObjHandle objectHandle,
                                                              ERRORINFO *errorInfo,
                                                              long index,
                                                              char **param);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseError2_GeterrorParametersCount (CAObjHandle objectHandle,
                                                                      ERRORINFO *errorInfo,
                                                                      long *count);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseErrorCollection_Getitem (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               long index,
                                                               ActiveXMLObj_IXMLDOMParseError2_ *error);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseErrorCollection_Getlength (CAObjHandle objectHandle,
                                                                 ERRORINFO *errorInfo,
                                                                 long *length);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseErrorCollection_Getnext (CAObjHandle objectHandle,
                                                               ERRORINFO *errorInfo,
                                                               ActiveXMLObj_IXMLDOMParseError2_ *error);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseErrorCollection_reset (CAObjHandle objectHandle,
                                                             ERRORINFO *errorInfo);

HRESULT CVIFUNC ActiveXML_IXMLDOMParseErrorCollection_Get_newEnum (CAObjHandle objectHandle,
                                                                   ERRORINFO *errorInfo,
                                                                   LPUNKNOWN *ppUnk);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_Getitem (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    long index,
                                                    ActiveXMLObj_IXMLDOMNode_ *listItem);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_Getlength (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo,
                                                      long *listLength);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_nextNode (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ *nextItem);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_reset (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_Get_newEnum (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        LPUNKNOWN *ppUnk);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_Getexpr (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    char **expression);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_Setexpr (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    const char *expression);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_Getcontext (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ *ppNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_SetByRefcontext (CAObjHandle objectHandle,
                                                            ERRORINFO *errorInfo,
                                                            ActiveXMLObj_IXMLDOMNode_ ppNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_peekNode (CAObjHandle objectHandle,
                                                     ERRORINFO *errorInfo,
                                                     ActiveXMLObj_IXMLDOMNode_ *ppNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_matches (CAObjHandle objectHandle,
                                                    ERRORINFO *errorInfo,
                                                    ActiveXMLObj_IXMLDOMNode_ pNode,
                                                    ActiveXMLObj_IXMLDOMNode_ *ppNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_removeNext (CAObjHandle objectHandle,
                                                       ERRORINFO *errorInfo,
                                                       ActiveXMLObj_IXMLDOMNode_ *ppNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_removeAll (CAObjHandle objectHandle,
                                                      ERRORINFO *errorInfo);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_clone (CAObjHandle objectHandle,
                                                  ERRORINFO *errorInfo,
                                                  ActiveXMLObj_IXMLDOMSelection_ *ppNode);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_getProperty (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *name,
                                                        VARIANT *value);

HRESULT CVIFUNC ActiveXML_IXMLDOMSelection_setProperty (CAObjHandle objectHandle,
                                                        ERRORINFO *errorInfo,
                                                        const char *name,
                                                        VARIANT value);
#ifdef __cplusplus
    }
#endif
#endif /* _ACTIVEXML_H */
