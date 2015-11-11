//
// Copyright 2006 The Android Open Source Project
//
// Build resource files from raw assets.
//

#include "ResourceTable.h"

#include "XMLNode.h"
#include "ResourceFilter.h"
#include "ResourceIdCache.h"

#include <androidfw/ResourceTypes.h>
#include <utils/ByteOrder.h>
#include <utils/TypeHelpers.h>
#include <stdarg.h>

#define NOISY(x) //x

status_t compileXmlFile(const Bundle* bundle,
                        const sp<AaptAssets>& assets,
                        const String16& resourceName,
                        const sp<AaptFile>& target,
                        ResourceTable* table,
                        int options)
{
    sp<XMLNode> root = XMLNode::parse(target);
    if (root == NULL) {
        return UNKNOWN_ERROR;
    }

    return compileXmlFile(bundle, assets, resourceName, root, target, table, options);
}

status_t compileXmlFile(const Bundle* bundle,
                        const sp<AaptAssets>& assets,
                        const String16& resourceName,
                        const sp<AaptFile>& target,
                        const sp<AaptFile>& outTarget,
                        ResourceTable* table,
                        int options)
{
    sp<XMLNode> root = XMLNode::parse(target);
    if (root == NULL) {
        return UNKNOWN_ERROR;
    }
    
    return compileXmlFile(bundle, assets, resourceName, root, outTarget, table, options);
}

status_t compileXmlFile(const Bundle* bundle,
                        const sp<AaptAssets>& assets,
                        const String16& resourceName,
                        const sp<XMLNode>& root,
                        const sp<AaptFile>& target,
                        ResourceTable* table,
                        int options)
{
    if ((options&XML_COMPILE_STRIP_WHITESPACE) != 0) {
        root->removeWhitespace(true, NULL);
    } else  if ((options&XML_COMPILE_COMPACT_WHITESPACE) != 0) {
        root->removeWhitespace(false, NULL);
    }

    if ((options&XML_COMPILE_UTF8) != 0) {
        root->setUTF8(true);
    }

    bool hasErrors = false;
    
    if ((options&XML_COMPILE_ASSIGN_ATTRIBUTE_IDS) != 0) {
        status_t err = root->assignResourceIds(assets, table);
        if (err != NO_ERROR) {
            fprintf(stderr,"assignResourceIds\n");
            hasErrors = true;
        }
    }

    status_t err = root->parseValues(assets, table);
    if (err != NO_ERROR) {
        fprintf(stderr,"parseValues\n");
        hasErrors = true;
    }

    if (hasErrors) {
        return UNKNOWN_ERROR;
    }

    if (table->modifyForCompat(bundle, resourceName, target, root) != NO_ERROR) {
        fprintf(stderr,"modifyForCompat\n");
        return UNKNOWN_ERROR;
    }
    
    NOISY(printf("Input XML Resource:\n"));
    NOISY(root->print());
    err = root->flatten(target,
            (options&XML_COMPILE_STRIP_COMMENTS) != 0,
            (options&XML_COMPILE_STRIP_RAW_VALUES) != 0);
    if (err != NO_ERROR) {
        fprintf(stderr,"flatten\n");
        return err;
    }

    NOISY(printf("Output XML Resource:\n"));
    NOISY(ResXMLTree tree;
        tree.setTo(target->getData(), target->getSize());
        printXMLBlock(&tree));

    target->setCompressionMethod(ZipEntry::kCompressDeflated);
    
    return err;
}

#undef NOISY
#define NOISY(x) //x

struct flag_entry
{
    const char16_t* name;
    size_t nameLen;
    uint32_t value;
    const char* description;
};

static const char16_t referenceArray[] =
    { 'r', 'e', 'f', 'e', 'r', 'e', 'n', 'c', 'e' };
static const char16_t stringArray[] =
    { 's', 't', 'r', 'i', 'n', 'g' };
static const char16_t integerArray[] =
    { 'i', 'n', 't', 'e', 'g', 'e', 'r' };
static const char16_t booleanArray[] =
    { 'b', 'o', 'o', 'l', 'e', 'a', 'n' };
static const char16_t colorArray[] =
    { 'c', 'o', 'l', 'o', 'r' };
static const char16_t floatArray[] =
    { 'f', 'l', 'o', 'a', 't' };
static const char16_t dimensionArray[] =
    { 'd', 'i', 'm', 'e', 'n', 's', 'i', 'o', 'n' };
static const char16_t fractionArray[] =
    { 'f', 'r', 'a', 'c', 't', 'i', 'o', 'n' };
static const char16_t enumArray[] =
    { 'e', 'n', 'u', 'm' };
static const char16_t flagsArray[] =
    { 'f', 'l', 'a', 'g', 's' };

static const flag_entry gFormatFlags[] = {
    { referenceArray, sizeof(referenceArray)/2, ResTable_map::TYPE_REFERENCE,
      "a reference to another resource, in the form \"<code>@[+][<i>package</i>:]<i>type</i>:<i>name</i></code>\"\n"
      "or to a theme attribute in the form \"<code>?[<i>package</i>:][<i>type</i>:]<i>name</i></code>\"."},
    { stringArray, sizeof(stringArray)/2, ResTable_map::TYPE_STRING,
      "a string value, using '\\\\;' to escape characters such as '\\\\n' or '\\\\uxxxx' for a unicode character." },
    { integerArray, sizeof(integerArray)/2, ResTable_map::TYPE_INTEGER,
      "an integer value, such as \"<code>100</code>\"." },
    { booleanArray, sizeof(booleanArray)/2, ResTable_map::TYPE_BOOLEAN,
      "a boolean value, either \"<code>true</code>\" or \"<code>false</code>\"." },
    { colorArray, sizeof(colorArray)/2, ResTable_map::TYPE_COLOR,
      "a color value, in the form of \"<code>#<i>rgb</i></code>\", \"<code>#<i>argb</i></code>\",\n"
      "\"<code>#<i>rrggbb</i></code>\", or \"<code>#<i>aarrggbb</i></code>\"." },
    { floatArray, sizeof(floatArray)/2, ResTable_map::TYPE_FLOAT,
      "a floating point value, such as \"<code>1.2</code>\"."},
    { dimensionArray, sizeof(dimensionArray)/2, ResTable_map::TYPE_DIMENSION,
      "a dimension value, which is a floating point number appended with a unit such as \"<code>14.5sp</code>\".\n"
      "Available units are: px (pixels), dp (density-independent pixels), sp (scaled pixels based on preferred font size),\n"
      "in (inches), mm (millimeters)." },
    { fractionArray, sizeof(fractionArray)/2, ResTable_map::TYPE_FRACTION,
      "a fractional value, which is a floating point number appended with either % or %p, such as \"<code>14.5%</code>\".\n"
      "The % suffix always means a percentage of the base size; the optional %p suffix provides a size relative to\n"
      "some parent container." },
    { enumArray, sizeof(enumArray)/2, ResTable_map::TYPE_ENUM, NULL },
    { flagsArray, sizeof(flagsArray)/2, ResTable_map::TYPE_FLAGS, NULL },
    { NULL, 0, 0, NULL }
};

static const char16_t suggestedArray[] = { 's', 'u', 'g', 'g', 'e', 's', 't', 'e', 'd' };

static const flag_entry l10nRequiredFlags[] = {
    { suggestedArray, sizeof(suggestedArray)/2, ResTable_map::L10N_SUGGESTED, NULL },
    { NULL, 0, 0, NULL }
};

static const char16_t nulStr[] = { 0 };

static uint32_t parse_flags(const char16_t* str, size_t len,
                             const flag_entry* flags, bool* outError = NULL)
{
    while (len > 0 && isspace(*str)) {
        str++;
        len--;
    }
    while (len > 0 && isspace(str[len-1])) {
        len--;
    }

    const char16_t* const end = str + len;
    uint32_t value = 0;

    while (str < end) {
        const char16_t* div = str;
        while (div < end && *div != '|') {
            div++;
        }

        const flag_entry* cur = flags;
        while (cur->name) {
            if (strzcmp16(cur->name, cur->nameLen, str, div-str) == 0) {
                value |= cur->value;
                break;
            }
            cur++;
        }

        if (!cur->name) {
            if (outError) *outError = true;
            return 0;
        }

        str = div < end ? div+1 : div;
    }

    if (outError) *outError = false;
    return value;
}

static String16 mayOrMust(int type, int flags)
{
    if ((type&(~flags)) == 0) {
        return String16("<p>Must");
    }
    
    return String16("<p>May");
}

static void appendTypeInfo(ResourceTable* outTable, const String16& pkg,
        const String16& typeName, const String16& ident, int type,
        const flag_entry* flags)
{
    bool hadType = false;
    while (flags->name) {
        if ((type&flags->value) != 0 && flags->description != NULL) {
            String16 fullMsg(mayOrMust(type, flags->value));
            fullMsg.append(String16(" be "));
            fullMsg.append(String16(flags->description));
            outTable->appendTypeComment(pkg, typeName, ident, fullMsg);
            hadType = true;
        }
        flags++;
    }
    if (hadType && (type&ResTable_map::TYPE_REFERENCE) == 0) {
        outTable->appendTypeComment(pkg, typeName, ident,
                String16("<p>This may also be a reference to a resource (in the form\n"
                         "\"<code>@[<i>package</i>:]<i>type</i>:<i>name</i></code>\") or\n"
                         "theme attribute (in the form\n"
                         "\"<code>?[<i>package</i>:][<i>type</i>:]<i>name</i></code>\")\n"
                         "containing a value of this type."));
    }
}

struct PendingAttribute
{
    const String16 myPackage;
    const SourcePos sourcePos;
    const bool appendComment;
    int32_t type;
    String16 ident;
    String16 comment;
    bool hasErrors;
    bool added;
    
    PendingAttribute(String16 _package, const sp<AaptFile>& in,
            ResXMLTree& block, bool _appendComment)
        : myPackage(_package)
        , sourcePos(in->getPrintableSource(), block.getLineNumber())
        , appendComment(_appendComment)
        , type(ResTable_map::TYPE_ANY)
        , hasErrors(false)
        , added(false)
    {
    }
    
    status_t createIfNeeded(ResourceTable* outTable)
    {
        if (added || hasErrors) {
            return NO_ERROR;
        }
        added = true;

        String16 attr16("attr");
        
        if (outTable->hasBagOrEntry(myPackage, attr16, ident)) {
            sourcePos.error("Attribute \"%s\" has already been defined\n",
                    String8(ident).string());
            hasErrors = true;
            return UNKNOWN_ERROR;
        }
        
        char numberStr[16];
        sprintf(numberStr, "%d", type);
        status_t err = outTable->addBag(sourcePos, myPackage,
                attr16, ident, String16(""),
                String16("^type"),
                String16(numberStr), NULL, NULL);
        if (err != NO_ERROR) {
            hasErrors = true;
            return err;
        }
        outTable->appendComment(myPackage, attr16, ident, comment, appendComment);
        //printf("Attribute %s comment: %s\n", String8(ident).string(),
        //     String8(comment).string());
        return err;
    }
};

static status_t compileAttribute(const sp<AaptFile>& in,
                                 ResXMLTree& block,
                                 const String16& myPackage,
                                 ResourceTable* outTable,
                                 String16* outIdent = NULL,
                                 bool inStyleable = false)
{
    PendingAttribute attr(myPackage, in, block, inStyleable);
    
    const String16 attr16("attr");
    const String16 id16("id");

    // Attribute type constants.
    const String16 enum16("enum");
    const String16 flag16("flag");

    ResXMLTree::event_code_t code;
    size_t len;
    status_t err;
    
    ssize_t identIdx = block.indexOfAttribute(NULL, "name");
    if (identIdx >= 0) {
        attr.ident = String16(block.getAttributeStringValue(identIdx, &len));
        if (outIdent) {
            *outIdent = attr.ident;
        }
    } else {
        attr.sourcePos.error("A 'name' attribute is required for <attr>\n");
        attr.hasErrors = true;
    }

    attr.comment = String16(
            block.getComment(&len) ? block.getComment(&len) : nulStr);

    ssize_t typeIdx = block.indexOfAttribute(NULL, "format");
    if (typeIdx >= 0) {
        String16 typeStr = String16(block.getAttributeStringValue(typeIdx, &len));
        attr.type = parse_flags(typeStr.string(), typeStr.size(), gFormatFlags);
        if (attr.type == 0) {
            attr.sourcePos.error("Tag <attr> 'format' attribute value \"%s\" not valid\n",
                    String8(typeStr).string());
            attr.hasErrors = true;
        }
        attr.createIfNeeded(outTable);
    } else if (!inStyleable) {
        // Attribute definitions outside of styleables always define the
        // attribute as a generic value.
        attr.createIfNeeded(outTable);
    }

    //printf("Attribute %s: type=0x%08x\n", String8(attr.ident).string(), attr.type);

    ssize_t minIdx = block.indexOfAttribute(NULL, "min");
    if (minIdx >= 0) {
        String16 val = String16(block.getAttributeStringValue(minIdx, &len));
        if (!ResTable::stringToInt(val.string(), val.size(), NULL)) {
            attr.sourcePos.error("Tag <attr> 'min' attribute must be a number, not \"%s\"\n",
                    String8(val).string());
            attr.hasErrors = true;
        }
        attr.createIfNeeded(outTable);
        if (!attr.hasErrors) {
            err = outTable->addBag(attr.sourcePos, myPackage, attr16, attr.ident,
                    String16(""), String16("^min"), String16(val), NULL, NULL);
            if (err != NO_ERROR) {
                attr.hasErrors = true;
            }
        }
    }

    ssize_t maxIdx = block.indexOfAttribute(NULL, "max");
    if (maxIdx >= 0) {
        String16 val = String16(block.getAttributeStringValue(maxIdx, &len));
        if (!ResTable::stringToInt(val.string(), val.size(), NULL)) {
            attr.sourcePos.error("Tag <attr> 'max' attribute must be a number, not \"%s\"\n",
                    String8(val).string());
            attr.hasErrors = true;
        }
        attr.createIfNeeded(outTable);
        if (!attr.hasErrors) {
            err = outTable->addBag(attr.sourcePos, myPackage, attr16, attr.ident,
                    String16(""), String16("^max"), String16(val), NULL, NULL);
            attr.hasErrors = true;
        }
    }

    if ((minIdx >= 0 || maxIdx >= 0) && (attr.type&ResTable_map::TYPE_INTEGER) == 0) {
        attr.sourcePos.error("Tag <attr> must have format=integer attribute if using max or min\n");
        attr.hasErrors = true;
    }

    ssize_t l10nIdx = block.indexOfAttribute(NULL, "localization");
    if (l10nIdx >= 0) {
        const uint16_t* str = block.getAttributeStringValue(l10nIdx, &len);
        bool error;
        uint32_t l10n_required = parse_flags(str, len, l10nRequiredFlags, &error);
        if (error) {
            attr.sourcePos.error("Tag <attr> 'localization' attribute value \"%s\" not valid\n",
                    String8(str).string());
            attr.hasErrors = true;
        }
        attr.createIfNeeded(outTable);
        if (!attr.hasErrors) {
            char buf[11];
            sprintf(buf, "%d", l10n_required);
            err = outTable->addBag(attr.sourcePos, myPackage, attr16, attr.ident,
                    String16(""), String16("^l10n"), String16(buf), NULL, NULL);
            if (err != NO_ERROR) {
                attr.hasErrors = true;
            }
        }
    }

    String16 enumOrFlagsComment;
    
    while ((code=block.next()) != ResXMLTree::END_DOCUMENT && code != ResXMLTree::BAD_DOCUMENT) {
        if (code == ResXMLTree::START_TAG) {
            uint32_t localType = 0;
            if (strcmp16(block.getElementName(&len), enum16.string()) == 0) {
                localType = ResTable_map::TYPE_ENUM;
            } else if (strcmp16(block.getElementName(&len), flag16.string()) == 0) {
                localType = ResTable_map::TYPE_FLAGS;
            } else {
                SourcePos(in->getPrintableSource(), block.getLineNumber())
                        .error("Tag <%s> can not appear inside <attr>, only <enum> or <flag>\n",
                        String8(block.getElementName(&len)).string());
                return UNKNOWN_ERROR;
            }

            attr.createIfNeeded(outTable);
            
            if (attr.type == ResTable_map::TYPE_ANY) {
                // No type was explicitly stated, so supplying enum tags
                // implicitly creates an enum or flag.
                attr.type = 0;
            }

            if ((attr.type&(ResTable_map::TYPE_ENUM|ResTable_map::TYPE_FLAGS)) == 0) {
                // Wasn't originally specified as an enum, so update its type.
                attr.type |= localType;
                if (!attr.hasErrors) {
                    char numberStr[16];
                    sprintf(numberStr, "%d", attr.type);
                    err = outTable->addBag(SourcePos(in->getPrintableSource(), block.getLineNumber()),
                            myPackage, attr16, attr.ident, String16(""),
                            String16("^type"), String16(numberStr), NULL, NULL, true);
                    if (err != NO_ERROR) {
                        attr.hasErrors = true;
                    }
                }
            } else if ((uint32_t)(attr.type&(ResTable_map::TYPE_ENUM|ResTable_map::TYPE_FLAGS)) != localType) {
                if (localType == ResTable_map::TYPE_ENUM) {
                    SourcePos(in->getPrintableSource(), block.getLineNumber())
                            .error("<enum> attribute can not be used inside a flags format\n");
                    attr.hasErrors = true;
                } else {
                    SourcePos(in->getPrintableSource(), block.getLineNumber())
                            .error("<flag> attribute can not be used inside a enum format\n");
                    attr.hasErrors = true;
                }
            }

            String16 itemIdent;
            ssize_t itemIdentIdx = block.indexOfAttribute(NULL, "name");
            if (itemIdentIdx >= 0) {
                itemIdent = String16(block.getAttributeStringValue(itemIdentIdx, &len));
            } else {
                SourcePos(in->getPrintableSource(), block.getLineNumber())
                        .error("A 'name' attribute is required for <enum> or <flag>\n");
                attr.hasErrors = true;
            }

            String16 value;
            ssize_t valueIdx = block.indexOfAttribute(NULL, "value");
            if (valueIdx >= 0) {
                value = String16(block.getAttributeStringValue(valueIdx, &len));
            } else {
                SourcePos(in->getPrintableSource(), block.getLineNumber())
                        .error("A 'value' attribute is required for <enum> or <flag>\n");
                attr.hasErrors = true;
            }
            if (!attr.hasErrors && !ResTable::stringToInt(value.string(), value.size(), NULL)) {
                SourcePos(in->getPrintableSource(), block.getLineNumber())
                        .error("Tag <enum> or <flag> 'value' attribute must be a number,"
                        " not \"%s\"\n",
                        String8(value).string());
                attr.hasErrors = true;
            }

            if (!attr.hasErrors) {
                if (enumOrFlagsComment.size() == 0) {
                    enumOrFlagsComment.append(mayOrMust(attr.type,
                            ResTable_map::TYPE_ENUM|ResTable_map::TYPE_FLAGS));
                    enumOrFlagsComment.append((attr.type&ResTable_map::TYPE_ENUM)
                                       ? String16(" be one of the following constant values.")
                                       : String16(" be one or more (separated by '|') of the following constant values."));
                    enumOrFlagsComment.append(String16("</p>\n<table>\n"
                                                "<colgroup align=\"left\" />\n"
                                                "<colgroup align=\"left\" />\n"
                                                "<colgroup align=\"left\" />\n"
                                                "<tr><th>Constant</th><th>Value</th><th>Description</th></tr>"));
                }
                
                enumOrFlagsComment.append(String16("\n<tr><td><code>"));
                enumOrFlagsComment.append(itemIdent);
                enumOrFlagsComment.append(String16("</code></td><td>"));
                enumOrFlagsComment.append(value);
                enumOrFlagsComment.append(String16("</td><td>"));
                if (block.getComment(&len)) {
                    enumOrFlagsComment.append(String16(block.getComment(&len)));
                }
                enumOrFlagsComment.append(String16("</td></tr>"));
                
                err = outTable->addBag(SourcePos(in->getPrintableSource(), block.getLineNumber()),
                                       myPackage,
                                       attr16, attr.ident, String16(""),
                                       itemIdent, value, NULL, NULL, false, true);
                if (err != NO_ERROR) {
                    attr.hasErrors = true;
                }
            }
        } else if (code == ResXMLTree::END_TAG) {
            if (strcmp16(block.getElementName(&len), attr16.string()) == 0) {
                break;
            }
            if ((attr.type&ResTable_map::TYPE_ENUM) != 0) {
                if (strcmp16(block.getElementName(&len), enum16.string()) != 0) {
                    SourcePos(in->getPrintableSource(), block.getLineNumber())
                            .error("Found tag </%s> where </enum> is expected\n",
                            String8(block.getElementName(&len)).string());
                    return UNKNOWN_ERROR;
                }
            } else {
                if (strcmp16(block.getElementName(&len), flag16.string()) != 0) {
                    SourcePos(in->getPrintableSource(), block.getLineNumber())
                            .error("Found tag </%s> where </flag> is expected\n",
                            String8(block.getElementName(&len)).string());
                    return UNKNOWN_ERROR;
                }
            }
        }
    }
    
    if (!attr.hasErrors && attr.added) {
        appendTypeInfo(outTable, myPackage, attr16, attr.ident, attr.type, gFormatFlags);
    }
    
    if (!attr.hasErrors && enumOrFlagsComment.size() > 0) {
        enumOrFlagsComment.append(String16("\n</table>"));
        outTable->appendTypeComment(myPackage, attr16, attr.ident, enumOrFlagsComment);
    }


    return NO_ERROR;
}

bool localeIsDefined(const ResTable_config& config)
{
    return config.locale == 0;
}

status_t parseAndAddBag(Bundle* bundle,
                        const sp<AaptFile>& in,
                        ResXMLTree* block,
                        const ResTable_config& config,
                        const String16& myPackage,
                        const String16& curType,
                        const String16& ident,
                        const String16& parentIdent,
                        const String16& itemIdent,
                        int32_t curFormat,
                        bool isFormatted,
                        const String16& product,
                        PseudolocalizationMethod pseudolocalize,
                        const bool overwrite,
                        ResourceTable* outTable)
{
    status_t err;
    const String16 item16("item");

    String16 str;
    Vector<StringPool::entry_style_span> spans;
    err = parseStyledString(bundle, in->getPrintableSource().string(),
                            block, item16, &str, &spans, isFormatted,
                            pseudolocalize);
    if (err != NO_ERROR) {
        return err;
    }
    
    NOISY(printf("Adding resource bag entry l=%c%c c=%c%c orien=%d d=%d "
                 " pid=%s, bag=%s, id=%s: %s\n",
                 config.language[0], config.language[1],
                 config.country[0], config.country[1],
                 config.orientation, config.density,
                 String8(parentIdent).string(),
                 String8(ident).string(),
                 String8(itemIdent).string(),
                 String8(str).string()));

    err = outTable->addBag(SourcePos(in->getPrintableSource(), block->getLineNumber()),
                           myPackage, curType, ident, parentIdent, itemIdent, str,
                           &spans, &config, overwrite, false, curFormat);
    return err;
}

/*
 * Returns true if needle is one of the elements in the comma-separated list
 * haystack, false otherwise.
 */
bool isInProductList(const String16& needle, const String16& haystack) {
    const char16_t *needle2 = needle.string();
    const char16_t *haystack2 = haystack.string();
    size_t needlesize = needle.size();

    while (*haystack2 != '\0') {
        if (strncmp16(haystack2, needle2, needlesize) == 0) {
            if (haystack2[needlesize] == '\0' || haystack2[needlesize] == ',') {
                return true;
            }
        }

        while (*haystack2 != '\0' && *haystack2 != ',') {
            haystack2++;
        }
        if (*haystack2 == ',') {
            haystack2++;
        }
    }

    return false;
}

/*
 * A simple container that holds a resource type and name. It is ordered first by type then
 * by name.
 */
struct type_ident_pair_t {
    String16 type;
    String16 ident;

    type_ident_pair_t() { };
    type_ident_pair_t(const String16& t, const String16& i) : type(t), ident(i) { }
    type_ident_pair_t(const type_ident_pair_t& o) : type(o.type), ident(o.ident) { }
    inline bool operator < (const type_ident_pair_t& o) const {
        int cmp = compare_type(type, o.type);
        if (cmp < 0) {
            return true;
        } else if (cmp > 0) {
            return false;
        } else {
            return strictly_order_type(ident, o.ident);
        }
    }
};


status_t parseAndAddEntry(Bundle* bundle,
                        const sp<AaptFile>& in,
                        ResXMLTree* block,
                        const ResTable_config& config,
                        const String16& myPackage,
                        const String16& curType,
                        const String16& ident,
                        const String16& curTag,
                        bool curIsStyled,
                        int32_t curFormat,
                        bool isFormatted,
                        const String16& product,
                        PseudolocalizationMethod pseudolocalize,
                        const bool overwrite,
                        KeyedVector<type_ident_pair_t, bool>* skippedResourceNames,
                        ResourceTable* outTable)
{
    status_t err;

    String16 str;
    Vector<StringPool::entry_style_span> spans;
    err = parseStyledString(bundle, in->getPrintableSource().string(), block,
                            curTag, &str, curIsStyled ? &spans : NULL,
                            isFormatted, pseudolocalize);

    if (err < NO_ERROR) { 
        return err;
    }

    /*
     * If a product type was specified on the command line
     * and also in the string, and the two are not the same,
     * return without adding the string.
     */

    const char *bundleProduct = bundle->getProduct();
    if (bundleProduct == NULL) {
        bundleProduct = "";
    }
    
    if (product.size() != 0) {
        /*
         * If the command-line-specified product is empty, only "default"
         * matches.  Other variants are skipped.  This is so generation
         * of the R.java file when the product is not known is predictable.
         */

        if (bundleProduct[0] == '\0') {
            if (strcmp16(String16("default").string(), product.string()) != 0) {
                /*
                 * This string has a product other than 'default'. Do not add it,
                 * but record it so that if we do not see the same string with
                 * product 'default' or no product, then report an error.
                 */
                skippedResourceNames->replaceValueFor(
                        type_ident_pair_t(curType, ident), true);
                return NO_ERROR;
            }
        } else {
            /*
             * The command-line product is not empty.
             * If the product for this string is on the command-line list,
             * it matches.  "default" also matches, but only if nothing
             * else has matched already.
             */

            if (isInProductList(product, String16(bundleProduct))) {
                ;
            } else if (strcmp16(String16("default").string(), product.string()) == 0 &&
                       !outTable->hasBagOrEntry(myPackage, curType, ident, config)) {
                ;
            } else {
                return NO_ERROR;
            }
        }
    }
    ;
   NOISY(printf("*********Adding resource entry l=%c%c c=%c%c orien=%d d=%d id=%s: %s\n",
    config.language[0], config.language[1],
    config.country[0], config.country[1],
    config.orientation, config.density,
    String8(ident).string(), String8(str).string()));

    err = outTable->addEntry(SourcePos(in->getPrintableSource(), block->getLineNumber()),
                             myPackage, curType, ident, str, &spans, &config,
                             false, curFormat, overwrite);

    return err;
}

status_t compileResourceFile(Bundle* bundle,
                             const sp<AaptAssets>& assets,
                             const sp<AaptFile>& in,
                             const ResTable_config& defParams,
                             const bool overwrite,
                             ResourceTable* outTable)
{
    ResXMLTree block;
    status_t err = parseXMLResource(in, &block, false, true);
    if (err != NO_ERROR) {
        return err;
    }

    // Top-level tag.
    const String16 resources16("resources");

    // Identifier declaration tags.
    const String16 declare_styleable16("declare-styleable");
    const String16 attr16("attr");

    // Data creation organizational tags.
    const String16 string16("string");
    const String16 drawable16("drawable");
    const String16 color16("color");
    const String16 bool16("bool");
    const String16 integer16("integer");
    const String16 dimen16("dimen");
    const String16 fraction16("fraction");
    const String16 style16("style");
    const String16 plurals16("plurals");
    const String16 array16("array");
    const String16 string_array16("string-array");
    const String16 integer_array16("integer-array");
    const String16 public16("public");
    const String16 public_padding16("public-padding");
    const String16 private_symbols16("private-symbols");
    const String16 java_symbol16("java-symbol");
    const String16 add_resource16("add-resource");
    const String16 skip16("skip");
    const String16 eat_comment16("eat-comment");

    // Data creation tags.
    const String16 bag16("bag");
    const String16 item16("item");

    // Attribute type constants.
    const String16 enum16("enum");

    // plural values
    const String16 other16("other");
    const String16 quantityOther16("^other");
    const String16 zero16("zero");
    const String16 quantityZero16("^zero");
    const String16 one16("one");
    const String16 quantityOne16("^one");
    const String16 two16("two");
    const String16 quantityTwo16("^two");
    const String16 few16("few");
    const String16 quantityFew16("^few");
    const String16 many16("many");
    const String16 quantityMany16("^many");

    // useful attribute names and special values
    const String16 name16("name");
    const String16 translatable16("translatable");
    const String16 formatted16("formatted");
    const String16 false16("false");

    const String16 myPackage(assets->getPackage());

    bool hasErrors = false;

    bool fileIsTranslatable = true;
    if (strstr(in->getPrintableSource().string(), "donottranslate") != NULL) {
        fileIsTranslatable = false;
    }

    DefaultKeyedVector<String16, uint32_t> nextPublicId(0);

    // Stores the resource names that were skipped. Typically this happens when
    // AAPT is invoked without a product specified and a resource has no
    // 'default' product attribute.
    KeyedVector<type_ident_pair_t, bool> skippedResourceNames;

    ResXMLTree::event_code_t code;
    do {
        code = block.next();
    } while (code == ResXMLTree::START_NAMESPACE);

    size_t len;
    if (code != ResXMLTree::START_TAG) {
        SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                "No start tag found\n");
        return UNKNOWN_ERROR;
    }
    if (strcmp16(block.getElementName(&len), resources16.string()) != 0) {
        SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                "Invalid start tag %s\n", String8(block.getElementName(&len)).string());
        return UNKNOWN_ERROR;
    }

    ResTable_config curParams(defParams);

    ResTable_config pseudoParams(curParams);
        pseudoParams.language[0] = 'e';
        pseudoParams.language[1] = 'n';
        pseudoParams.country[0] = 'X';
        pseudoParams.country[1] = 'A';

    ResTable_config pseudoBidiParams(curParams);
        pseudoBidiParams.language[0] = 'a';
        pseudoBidiParams.language[1] = 'r';
        pseudoBidiParams.country[0] = 'X';
        pseudoBidiParams.country[1] = 'B';

    // We should skip resources for pseudolocales if they were
    // already added automatically. This is a fix for a transition period when
    // manually pseudolocalized resources may be expected.
    // TODO: remove this check after next SDK version release.
    if ((bundle->getPseudolocalize() & PSEUDO_ACCENTED &&
         curParams.locale == pseudoParams.locale) ||
        (bundle->getPseudolocalize() & PSEUDO_BIDI &&
         curParams.locale == pseudoBidiParams.locale)) {
        SourcePos(in->getPrintableSource(), 0).warning(
                "Resource file %s is skipped as pseudolocalization"
                " was done automatically.",
                in->getPrintableSource().string());
        return NO_ERROR;
    }

    while ((code=block.next()) != ResXMLTree::END_DOCUMENT && code != ResXMLTree::BAD_DOCUMENT) {
        if (code == ResXMLTree::START_TAG) {
            const String16* curTag = NULL;
            String16 curType;
            int32_t curFormat = ResTable_map::TYPE_ANY;
            bool curIsBag = false;
            bool curIsBagReplaceOnOverwrite = false;
            bool curIsStyled = false;
            bool curIsPseudolocalizable = false;
            bool curIsFormatted = fileIsTranslatable;
            bool localHasErrors = false;

            if (strcmp16(block.getElementName(&len), skip16.string()) == 0) {
                while ((code=block.next()) != ResXMLTree::END_DOCUMENT
                        && code != ResXMLTree::BAD_DOCUMENT) {
                    if (code == ResXMLTree::END_TAG) {
                        if (strcmp16(block.getElementName(&len), skip16.string()) == 0) {
                            break;
                        }
                    }
                }
                continue;

            } else if (strcmp16(block.getElementName(&len), eat_comment16.string()) == 0) {
                while ((code=block.next()) != ResXMLTree::END_DOCUMENT
                        && code != ResXMLTree::BAD_DOCUMENT) {
                    if (code == ResXMLTree::END_TAG) {
                        if (strcmp16(block.getElementName(&len), eat_comment16.string()) == 0) {
                            break;
                        }
                    }
                }
                continue;

            } else if (strcmp16(block.getElementName(&len), public16.string()) == 0) {
                SourcePos srcPos(in->getPrintableSource(), block.getLineNumber());
            
                String16 type;
                ssize_t typeIdx = block.indexOfAttribute(NULL, "type");
                if (typeIdx < 0) {
                    srcPos.error("A 'type' attribute is required for <public>\n");
                    hasErrors = localHasErrors = true;
                }
                type = String16(block.getAttributeStringValue(typeIdx, &len));

                String16 name;
                ssize_t nameIdx = block.indexOfAttribute(NULL, "name");
                if (nameIdx < 0) {
                    srcPos.error("A 'name' attribute is required for <public>\n");
                    hasErrors = localHasErrors = true;
                }
                name = String16(block.getAttributeStringValue(nameIdx, &len));

                uint32_t ident = 0;
                ssize_t identIdx = block.indexOfAttribute(NULL, "id");
                if (identIdx >= 0) {
                    const char16_t* identStr = block.getAttributeStringValue(identIdx, &len);
                    Res_value identValue;
                    if (!ResTable::stringToInt(identStr, len, &identValue)) {
                        srcPos.error("Given 'id' attribute is not an integer: %s\n",
                                String8(block.getAttributeStringValue(identIdx, &len)).string());
                        hasErrors = localHasErrors = true;
                    } else {
                        ident = identValue.data;
                        nextPublicId.replaceValueFor(type, ident+1);
                    }
                } else if (nextPublicId.indexOfKey(type) < 0) {
                    srcPos.error("No 'id' attribute supplied <public>,"
                            " and no previous id defined in this file.\n");
                    hasErrors = localHasErrors = true;
                } else if (!localHasErrors) {
                    ident = nextPublicId.valueFor(type);
                    nextPublicId.replaceValueFor(type, ident+1);
                }

                if (!localHasErrors) {
                    err = outTable->addPublic(srcPos, myPackage, type, name, ident);
                    if (err < NO_ERROR) {
                        hasErrors = localHasErrors = true;
                    }
                }
                if (!localHasErrors) {
                    sp<AaptSymbols> symbols = assets->getSymbolsFor(String8("R"));
                    if (symbols != NULL) {
                        symbols = symbols->addNestedSymbol(String8(type), srcPos);
                    }
                    if (symbols != NULL) {
                        symbols->makeSymbolPublic(String8(name), srcPos);
                        String16 comment(
                            block.getComment(&len) ? block.getComment(&len) : nulStr);
                        symbols->appendComment(String8(name), comment, srcPos);
                    } else {
                        srcPos.error("Unable to create symbols!\n");
                        hasErrors = localHasErrors = true;
                    }
                }

                while ((code=block.next()) != ResXMLTree::END_DOCUMENT && code != ResXMLTree::BAD_DOCUMENT) {
                    if (code == ResXMLTree::END_TAG) {
                        if (strcmp16(block.getElementName(&len), public16.string()) == 0) {
                            break;
                        }
                    }
                }
                continue;

            } else if (strcmp16(block.getElementName(&len), public_padding16.string()) == 0) {
                SourcePos srcPos(in->getPrintableSource(), block.getLineNumber());
            
                String16 type;
                ssize_t typeIdx = block.indexOfAttribute(NULL, "type");
                if (typeIdx < 0) {
                    srcPos.error("A 'type' attribute is required for <public-padding>\n");
                    hasErrors = localHasErrors = true;
                }
                type = String16(block.getAttributeStringValue(typeIdx, &len));

                String16 name;
                ssize_t nameIdx = block.indexOfAttribute(NULL, "name");
                if (nameIdx < 0) {
                    srcPos.error("A 'name' attribute is required for <public-padding>\n");
                    hasErrors = localHasErrors = true;
                }
                name = String16(block.getAttributeStringValue(nameIdx, &len));

                uint32_t start = 0;
                ssize_t startIdx = block.indexOfAttribute(NULL, "start");
                if (startIdx >= 0) {
                    const char16_t* startStr = block.getAttributeStringValue(startIdx, &len);
                    Res_value startValue;
                    if (!ResTable::stringToInt(startStr, len, &startValue)) {
                        srcPos.error("Given 'start' attribute is not an integer: %s\n",
                                String8(block.getAttributeStringValue(startIdx, &len)).string());
                        hasErrors = localHasErrors = true;
                    } else {
                        start = startValue.data;
                    }
                } else if (nextPublicId.indexOfKey(type) < 0) {
                    srcPos.error("No 'start' attribute supplied <public-padding>,"
                            " and no previous id defined in this file.\n");
                    hasErrors = localHasErrors = true;
                } else if (!localHasErrors) {
                    start = nextPublicId.valueFor(type);
                }

                uint32_t end = 0;
                ssize_t endIdx = block.indexOfAttribute(NULL, "end");
                if (endIdx >= 0) {
                    const char16_t* endStr = block.getAttributeStringValue(endIdx, &len);
                    Res_value endValue;
                    if (!ResTable::stringToInt(endStr, len, &endValue)) {
                        srcPos.error("Given 'end' attribute is not an integer: %s\n",
                                String8(block.getAttributeStringValue(endIdx, &len)).string());
                        hasErrors = localHasErrors = true;
                    } else {
                        end = endValue.data;
                    }
                } else {
                    srcPos.error("No 'end' attribute supplied <public-padding>\n");
                    hasErrors = localHasErrors = true;
                }

                if (end >= start) {
                    nextPublicId.replaceValueFor(type, end+1);
                } else {
                    srcPos.error("Padding start '%ul' is after end '%ul'\n",
                            start, end);
                    hasErrors = localHasErrors = true;
                }
                
                String16 comment(
                    block.getComment(&len) ? block.getComment(&len) : nulStr);
                for (uint32_t curIdent=start; curIdent<=end; curIdent++) {
                    if (localHasErrors) {
                        break;
                    }
                    String16 curName(name);
                    char buf[64];
                    sprintf(buf, "%d", (int)(end-curIdent+1));
                    curName.append(String16(buf));
                    
                    err = outTable->addEntry(srcPos, myPackage, type, curName,
                                             String16("padding"), NULL, &curParams, false,
                                             ResTable_map::TYPE_STRING, overwrite);
                    if (err < NO_ERROR) {
                        hasErrors = localHasErrors = true;
                        break;
                    }
                    err = outTable->addPublic(srcPos, myPackage, type,
                            curName, curIdent);
                    if (err < NO_ERROR) {
                        hasErrors = localHasErrors = true;
                        break;
                    }
                    sp<AaptSymbols> symbols = assets->getSymbolsFor(String8("R"));
                    if (symbols != NULL) {
                        symbols = symbols->addNestedSymbol(String8(type), srcPos);
                    }
                    if (symbols != NULL) {
                        symbols->makeSymbolPublic(String8(curName), srcPos);
                        symbols->appendComment(String8(curName), comment, srcPos);
                    } else {
                        srcPos.error("Unable to create symbols!\n");
                        hasErrors = localHasErrors = true;
                    }
                }

                while ((code=block.next()) != ResXMLTree::END_DOCUMENT && code != ResXMLTree::BAD_DOCUMENT) {
                    if (code == ResXMLTree::END_TAG) {
                        if (strcmp16(block.getElementName(&len), public_padding16.string()) == 0) {
                            break;
                        }
                    }
                }
                continue;

            } else if (strcmp16(block.getElementName(&len), private_symbols16.string()) == 0) {
                String16 pkg;
                ssize_t pkgIdx = block.indexOfAttribute(NULL, "package");
                if (pkgIdx < 0) {
                    SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                            "A 'package' attribute is required for <private-symbols>\n");
                    hasErrors = localHasErrors = true;
                }
                pkg = String16(block.getAttributeStringValue(pkgIdx, &len));
                if (!localHasErrors) {
                    assets->setSymbolsPrivatePackage(String8(pkg));
                }

                while ((code=block.next()) != ResXMLTree::END_DOCUMENT && code != ResXMLTree::BAD_DOCUMENT) {
                    if (code == ResXMLTree::END_TAG) {
                        if (strcmp16(block.getElementName(&len), private_symbols16.string()) == 0) {
                            break;
                        }
                    }
                }
                continue;

            } else if (strcmp16(block.getElementName(&len), java_symbol16.string()) == 0) {
                SourcePos srcPos(in->getPrintableSource(), block.getLineNumber());
            
                String16 type;
                ssize_t typeIdx = block.indexOfAttribute(NULL, "type");
                if (typeIdx < 0) {
                    srcPos.error("A 'type' attribute is required for <public>\n");
                    hasErrors = localHasErrors = true;
                }
                type = String16(block.getAttributeStringValue(typeIdx, &len));

                String16 name;
                ssize_t nameIdx = block.indexOfAttribute(NULL, "name");
                if (nameIdx < 0) {
                    srcPos.error("A 'name' attribute is required for <public>\n");
                    hasErrors = localHasErrors = true;
                }
                name = String16(block.getAttributeStringValue(nameIdx, &len));

                sp<AaptSymbols> symbols = assets->getJavaSymbolsFor(String8("R"));
                if (symbols != NULL) {
                    symbols = symbols->addNestedSymbol(String8(type), srcPos);
                }
                if (symbols != NULL) {
                    symbols->makeSymbolJavaSymbol(String8(name), srcPos);
                    String16 comment(
                        block.getComment(&len) ? block.getComment(&len) : nulStr);
                    symbols->appendComment(String8(name), comment, srcPos);
                } else {
                    srcPos.error("Unable to create symbols!\n");
                    hasErrors = localHasErrors = true;
                }

                while ((code=block.next()) != ResXMLTree::END_DOCUMENT && code != ResXMLTree::BAD_DOCUMENT) {
                    if (code == ResXMLTree::END_TAG) {
                        if (strcmp16(block.getElementName(&len), java_symbol16.string()) == 0) {
                            break;
                        }
                    }
                }
                continue;


            } else if (strcmp16(block.getElementName(&len), add_resource16.string()) == 0) {
                SourcePos srcPos(in->getPrintableSource(), block.getLineNumber());
            
                String16 typeName;
                ssize_t typeIdx = block.indexOfAttribute(NULL, "type");
                if (typeIdx < 0) {
                    srcPos.error("A 'type' attribute is required for <add-resource>\n");
                    hasErrors = localHasErrors = true;
                }
                typeName = String16(block.getAttributeStringValue(typeIdx, &len));

                String16 name;
                ssize_t nameIdx = block.indexOfAttribute(NULL, "name");
                if (nameIdx < 0) {
                    srcPos.error("A 'name' attribute is required for <add-resource>\n");
                    hasErrors = localHasErrors = true;
                }
                name = String16(block.getAttributeStringValue(nameIdx, &len));

                outTable->canAddEntry(srcPos, myPackage, typeName, name);

                while ((code=block.next()) != ResXMLTree::END_DOCUMENT && code != ResXMLTree::BAD_DOCUMENT) {
                    if (code == ResXMLTree::END_TAG) {
                        if (strcmp16(block.getElementName(&len), add_resource16.string()) == 0) {
                            break;
                        }
                    }
                }
                continue;
                
            } else if (strcmp16(block.getElementName(&len), declare_styleable16.string()) == 0) {
                SourcePos srcPos(in->getPrintableSource(), block.getLineNumber());
                                
                String16 ident;
                ssize_t identIdx = block.indexOfAttribute(NULL, "name");
                if (identIdx < 0) {
                    srcPos.error("A 'name' attribute is required for <declare-styleable>\n");
                    hasErrors = localHasErrors = true;
                }
                ident = String16(block.getAttributeStringValue(identIdx, &len));

                sp<AaptSymbols> symbols = assets->getSymbolsFor(String8("R"));
                if (!localHasErrors) {
                    if (symbols != NULL) {
                        symbols = symbols->addNestedSymbol(String8("styleable"), srcPos);
                    }
                    sp<AaptSymbols> styleSymbols = symbols;
                    if (symbols != NULL) {
                        symbols = symbols->addNestedSymbol(String8(ident), srcPos);
                    }
                    if (symbols == NULL) {
                        srcPos.error("Unable to create symbols!\n");
                        return UNKNOWN_ERROR;
                    }
                    
                    String16 comment(
                        block.getComment(&len) ? block.getComment(&len) : nulStr);
                    styleSymbols->appendComment(String8(ident), comment, srcPos);
                } else {
                    symbols = NULL;
                }

                while ((code=block.next()) != ResXMLTree::END_DOCUMENT && code != ResXMLTree::BAD_DOCUMENT) {
                    if (code == ResXMLTree::START_TAG) {
                        if (strcmp16(block.getElementName(&len), skip16.string()) == 0) {
                            while ((code=block.next()) != ResXMLTree::END_DOCUMENT
                                   && code != ResXMLTree::BAD_DOCUMENT) {
                                if (code == ResXMLTree::END_TAG) {
                                    if (strcmp16(block.getElementName(&len), skip16.string()) == 0) {
                                        break;
                                    }
                                }
                            }
                            continue;
                        } else if (strcmp16(block.getElementName(&len), eat_comment16.string()) == 0) {
                            while ((code=block.next()) != ResXMLTree::END_DOCUMENT
                                   && code != ResXMLTree::BAD_DOCUMENT) {
                                if (code == ResXMLTree::END_TAG) {
                                    if (strcmp16(block.getElementName(&len), eat_comment16.string()) == 0) {
                                        break;
                                    }
                                }
                            }
                            continue;
                        } else if (strcmp16(block.getElementName(&len), attr16.string()) != 0) {
                            SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                                    "Tag <%s> can not appear inside <declare-styleable>, only <attr>\n",
                                    String8(block.getElementName(&len)).string());
                            return UNKNOWN_ERROR;
                        }

                        String16 comment(
                            block.getComment(&len) ? block.getComment(&len) : nulStr);
                        String16 itemIdent;
                        err = compileAttribute(in, block, myPackage, outTable, &itemIdent, true);
                        if (err != NO_ERROR) {
                            hasErrors = localHasErrors = true;
                        }

                        if (symbols != NULL) {
                            SourcePos srcPos(String8(in->getPrintableSource()), block.getLineNumber());
                            symbols->addSymbol(String8(itemIdent), 0, srcPos);
                            symbols->appendComment(String8(itemIdent), comment, srcPos);
                            //printf("Attribute %s comment: %s\n", String8(itemIdent).string(),
                            //     String8(comment).string());
                        }
                    } else if (code == ResXMLTree::END_TAG) {
                        if (strcmp16(block.getElementName(&len), declare_styleable16.string()) == 0) {
                            break;
                        }

                        SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                                "Found tag </%s> where </attr> is expected\n",
                                String8(block.getElementName(&len)).string());
                        return UNKNOWN_ERROR;
                    }
                }
                continue;

            } else if (strcmp16(block.getElementName(&len), attr16.string()) == 0) {
                err = compileAttribute(in, block, myPackage, outTable, NULL);
                if (err != NO_ERROR) {
                    hasErrors = true;
                }
                continue;

            } else if (strcmp16(block.getElementName(&len), item16.string()) == 0) {
                curTag = &item16;
                ssize_t attri = block.indexOfAttribute(NULL, "type");
                if (attri >= 0) {
                    curType = String16(block.getAttributeStringValue(attri, &len));
                    ssize_t formatIdx = block.indexOfAttribute(NULL, "format");
                    if (formatIdx >= 0) {
                        String16 formatStr = String16(block.getAttributeStringValue(
                                formatIdx, &len));
                        curFormat = parse_flags(formatStr.string(), formatStr.size(),
                                                gFormatFlags);
                        if (curFormat == 0) {
                            SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                                    "Tag <item> 'format' attribute value \"%s\" not valid\n",
                                    String8(formatStr).string());
                            hasErrors = localHasErrors = true;
                        }
                    }
                } else {
                    SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                            "A 'type' attribute is required for <item>\n");
                    hasErrors = localHasErrors = true;
                }
                curIsStyled = true;
            } else if (strcmp16(block.getElementName(&len), string16.string()) == 0) {
                // Note the existence and locale of every string we process
                char rawLocale[RESTABLE_MAX_LOCALE_LEN];
                curParams.getBcp47Locale(rawLocale);
                String8 locale(rawLocale);
                String16 name;
                String16 translatable;
                String16 formatted;

                size_t n = block.getAttributeCount();
                for (size_t i = 0; i < n; i++) {
                    size_t length;
                    const uint16_t* attr = block.getAttributeName(i, &length);
                    if (strcmp16(attr, name16.string()) == 0) {
                        name.setTo(block.getAttributeStringValue(i, &length));
                    } else if (strcmp16(attr, translatable16.string()) == 0) {
                        translatable.setTo(block.getAttributeStringValue(i, &length));
                    } else if (strcmp16(attr, formatted16.string()) == 0) {
                        formatted.setTo(block.getAttributeStringValue(i, &length));
                    }
                }
                
                if (name.size() > 0) {
                    if (translatable == false16) {
                        curIsFormatted = false;
                        // Untranslatable strings must only exist in the default [empty] locale
                        if (locale.size() > 0) {
                            SourcePos(in->getPrintableSource(), block.getLineNumber()).warning(
                                    "string '%s' marked untranslatable but exists in locale '%s'\n",
                                    String8(name).string(),
                                    locale.string());
                            // hasErrors = localHasErrors = true;
                        } else {
                            // Intentionally empty block:
                            //
                            // Don't add untranslatable strings to the localization table; that
                            // way if we later see localizations of them, they'll be flagged as
                            // having no default translation.
                        }
                    } else {
                        outTable->addLocalization(
                                name,
                                locale,
                                SourcePos(in->getPrintableSource(), block.getLineNumber()));
                    }

                    if (formatted == false16) {
                        curIsFormatted = false;
                    }
                }

                curTag = &string16;
                curType = string16;
                curFormat = ResTable_map::TYPE_REFERENCE|ResTable_map::TYPE_STRING;
                curIsStyled = true;
                curIsPseudolocalizable = fileIsTranslatable && (translatable != false16);
            } else if (strcmp16(block.getElementName(&len), drawable16.string()) == 0) {
                curTag = &drawable16;
                curType = drawable16;
                curFormat = ResTable_map::TYPE_REFERENCE|ResTable_map::TYPE_COLOR;
            } else if (strcmp16(block.getElementName(&len), color16.string()) == 0) {
                curTag = &color16;
                curType = color16;
                curFormat = ResTable_map::TYPE_REFERENCE|ResTable_map::TYPE_COLOR;
            } else if (strcmp16(block.getElementName(&len), bool16.string()) == 0) {
                curTag = &bool16;
                curType = bool16;
                curFormat = ResTable_map::TYPE_REFERENCE|ResTable_map::TYPE_BOOLEAN;
            } else if (strcmp16(block.getElementName(&len), integer16.string()) == 0) {
                curTag = &integer16;
                curType = integer16;
                curFormat = ResTable_map::TYPE_REFERENCE|ResTable_map::TYPE_INTEGER;
            } else if (strcmp16(block.getElementName(&len), dimen16.string()) == 0) {
                curTag = &dimen16;
                curType = dimen16;
                curFormat = ResTable_map::TYPE_REFERENCE|ResTable_map::TYPE_DIMENSION;
            } else if (strcmp16(block.getElementName(&len), fraction16.string()) == 0) {
                curTag = &fraction16;
                curType = fraction16;
                curFormat = ResTable_map::TYPE_REFERENCE|ResTable_map::TYPE_FRACTION;
            } else if (strcmp16(block.getElementName(&len), bag16.string()) == 0) {
                curTag = &bag16;
                curIsBag = true;
                ssize_t attri = block.indexOfAttribute(NULL, "type");
                if (attri >= 0) {
                    curType = String16(block.getAttributeStringValue(attri, &len));
                } else {
                    SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                            "A 'type' attribute is required for <bag>\n");
                    hasErrors = localHasErrors = true;
                }
            } else if (strcmp16(block.getElementName(&len), style16.string()) == 0) {
                curTag = &style16;
                curType = style16;
                curIsBag = true;
            } else if (strcmp16(block.getElementName(&len), plurals16.string()) == 0) {
                curTag = &plurals16;
                curType = plurals16;
                curIsBag = true;
                curIsPseudolocalizable = fileIsTranslatable;
            } else if (strcmp16(block.getElementName(&len), array16.string()) == 0) {
                curTag = &array16;
                curType = array16;
                curIsBag = true;
                curIsBagReplaceOnOverwrite = true;
                ssize_t formatIdx = block.indexOfAttribute(NULL, "format");
                if (formatIdx >= 0) {
                    String16 formatStr = String16(block.getAttributeStringValue(
                            formatIdx, &len));
                    curFormat = parse_flags(formatStr.string(), formatStr.size(),
                                            gFormatFlags);
                    if (curFormat == 0) {
                        SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                                "Tag <array> 'format' attribute value \"%s\" not valid\n",
                                String8(formatStr).string());
                        hasErrors = localHasErrors = true;
                    }
                }
            } else if (strcmp16(block.getElementName(&len), string_array16.string()) == 0) {
                // Check whether these strings need valid formats.
                // (simplified form of what string16 does above)
                bool isTranslatable = false;
                size_t n = block.getAttributeCount();

                // Pseudolocalizable by default, unless this string array isn't
                // translatable.
                for (size_t i = 0; i < n; i++) {
                    size_t length;
                    const uint16_t* attr = block.getAttributeName(i, &length);
                    if (strcmp16(attr, formatted16.string()) == 0) {
                        const uint16_t* value = block.getAttributeStringValue(i, &length);
                        if (strcmp16(value, false16.string()) == 0) {
                            curIsFormatted = false;
                        }
                    } else if (strcmp16(attr, translatable16.string()) == 0) {
                        const uint16_t* value = block.getAttributeStringValue(i, &length);
                        if (strcmp16(value, false16.string()) == 0) {
                            isTranslatable = false;
                        }
                    }
                }

                curTag = &string_array16;
                curType = array16;
                curFormat = ResTable_map::TYPE_REFERENCE|ResTable_map::TYPE_STRING;
                curIsBag = true;
                curIsBagReplaceOnOverwrite = true;
                curIsPseudolocalizable = isTranslatable && fileIsTranslatable;
            } else if (strcmp16(block.getElementName(&len), integer_array16.string()) == 0) {
                curTag = &integer_array16;
                curType = array16;
                curFormat = ResTable_map::TYPE_REFERENCE|ResTable_map::TYPE_INTEGER;
                curIsBag = true;
                curIsBagReplaceOnOverwrite = true;
            } else {
                SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                        "Found tag %s where item is expected\n",
                        String8(block.getElementName(&len)).string());
                return UNKNOWN_ERROR;
            }

            String16 ident;
            ssize_t identIdx = block.indexOfAttribute(NULL, "name");
            if (identIdx >= 0) {
                ident = String16(block.getAttributeStringValue(identIdx, &len));
            } else {
                SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                        "A 'name' attribute is required for <%s>\n",
                        String8(*curTag).string());
                hasErrors = localHasErrors = true;
            }

            String16 product;
            identIdx = block.indexOfAttribute(NULL, "product");
            if (identIdx >= 0) {
                product = String16(block.getAttributeStringValue(identIdx, &len));
            }

            String16 comment(block.getComment(&len) ? block.getComment(&len) : nulStr);
            
            if (curIsBag) {
                // Figure out the parent of this bag...
                String16 parentIdent;
                ssize_t parentIdentIdx = block.indexOfAttribute(NULL, "parent");
                if (parentIdentIdx >= 0) {
                    parentIdent = String16(block.getAttributeStringValue(parentIdentIdx, &len));
                } else {
                    ssize_t sep = ident.findLast('.');
                    if (sep >= 0) {
                        parentIdent.setTo(ident, sep);
                    }
                }

                if (!localHasErrors) {
                    err = outTable->startBag(SourcePos(in->getPrintableSource(),
                            block.getLineNumber()), myPackage, curType, ident,
                            parentIdent, &curParams,
                            overwrite, curIsBagReplaceOnOverwrite);
                    if (err != NO_ERROR) {
                        hasErrors = localHasErrors = true;
                    }
                }
                
                ssize_t elmIndex = 0;
                char elmIndexStr[14];
                while ((code=block.next()) != ResXMLTree::END_DOCUMENT
                        && code != ResXMLTree::BAD_DOCUMENT) {

                    if (code == ResXMLTree::START_TAG) {
                        if (strcmp16(block.getElementName(&len), item16.string()) != 0) {
                            SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                                    "Tag <%s> can not appear inside <%s>, only <item>\n",
                                    String8(block.getElementName(&len)).string(),
                                    String8(*curTag).string());
                            return UNKNOWN_ERROR;
                        }

                        String16 itemIdent;
                        if (curType == array16) {
                            sprintf(elmIndexStr, "^index_%d", (int)elmIndex++);
                            itemIdent = String16(elmIndexStr);
                        } else if (curType == plurals16) {
                            ssize_t itemIdentIdx = block.indexOfAttribute(NULL, "quantity");
                            if (itemIdentIdx >= 0) {
                                String16 quantity16(block.getAttributeStringValue(itemIdentIdx, &len));
                                if (quantity16 == other16) {
                                    itemIdent = quantityOther16;
                                }
                                else if (quantity16 == zero16) {
                                    itemIdent = quantityZero16;
                                }
                                else if (quantity16 == one16) {
                                    itemIdent = quantityOne16;
                                }
                                else if (quantity16 == two16) {
                                    itemIdent = quantityTwo16;
                                }
                                else if (quantity16 == few16) {
                                    itemIdent = quantityFew16;
                                }
                                else if (quantity16 == many16) {
                                    itemIdent = quantityMany16;
                                }
                                else {
                                    SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                                            "Illegal 'quantity' attribute is <item> inside <plurals>\n");
                                    hasErrors = localHasErrors = true;
                                }
                            } else {
                                SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                                        "A 'quantity' attribute is required for <item> inside <plurals>\n");
                                hasErrors = localHasErrors = true;
                            }
                        } else {
                            ssize_t itemIdentIdx = block.indexOfAttribute(NULL, "name");
                            if (itemIdentIdx >= 0) {
                                itemIdent = String16(block.getAttributeStringValue(itemIdentIdx, &len));
                            } else {
                                SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                                        "A 'name' attribute is required for <item>\n");
                                hasErrors = localHasErrors = true;
                            }
                        }

                        ResXMLParser::ResXMLPosition parserPosition;
                        block.getPosition(&parserPosition);

                        err = parseAndAddBag(bundle, in, &block, curParams, myPackage, curType,
                                ident, parentIdent, itemIdent, curFormat, curIsFormatted,
                                product, NO_PSEUDOLOCALIZATION, overwrite, outTable);
                        if (err == NO_ERROR) {
                            if (curIsPseudolocalizable && localeIsDefined(curParams)
                                    && bundle->getPseudolocalize() > 0) {
                                // pseudolocalize here
                                if ((PSEUDO_ACCENTED & bundle->getPseudolocalize()) ==
                                   PSEUDO_ACCENTED) {
                                    block.setPosition(parserPosition);
                                    err = parseAndAddBag(bundle, in, &block, pseudoParams, myPackage,
                                            curType, ident, parentIdent, itemIdent, curFormat,
                                            curIsFormatted, product, PSEUDO_ACCENTED,
                                            overwrite, outTable);
                                }
                                if ((PSEUDO_BIDI & bundle->getPseudolocalize()) ==
                                   PSEUDO_BIDI) {
                                    block.setPosition(parserPosition);
                                    err = parseAndAddBag(bundle, in, &block, pseudoBidiParams, myPackage,
                                            curType, ident, parentIdent, itemIdent, curFormat,
                                            curIsFormatted, product, PSEUDO_BIDI,
                                            overwrite, outTable);
                                }
                            }
                        }
                        if (err != NO_ERROR) {
                            hasErrors = localHasErrors = true;
                        }
                    } else if (code == ResXMLTree::END_TAG) {
                        if (strcmp16(block.getElementName(&len), curTag->string()) != 0) {
                            SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                                    "Found tag </%s> where </%s> is expected\n",
                                    String8(block.getElementName(&len)).string(),
                                    String8(*curTag).string());
                            return UNKNOWN_ERROR;
                        }
                        break;
                    }
                }
            } else {
                ResXMLParser::ResXMLPosition parserPosition;
                block.getPosition(&parserPosition);

                err = parseAndAddEntry(bundle, in, &block, curParams, myPackage, curType, ident,
                        *curTag, curIsStyled, curFormat, curIsFormatted,
                        product, NO_PSEUDOLOCALIZATION, overwrite, &skippedResourceNames, outTable);

                if (err < NO_ERROR) { // Why err < NO_ERROR instead of err != NO_ERROR?
                    hasErrors = localHasErrors = true;
                }
                else if (err == NO_ERROR) {
                    if (curIsPseudolocalizable && localeIsDefined(curParams)
                            && bundle->getPseudolocalize() > 0) {
                        // pseudolocalize here
                        if ((PSEUDO_ACCENTED & bundle->getPseudolocalize()) ==
                           PSEUDO_ACCENTED) {
                            block.setPosition(parserPosition);
                            err = parseAndAddEntry(bundle, in, &block, pseudoParams, myPackage, curType,
                                    ident, *curTag, curIsStyled, curFormat,
                                    curIsFormatted, product,
                                    PSEUDO_ACCENTED, overwrite, &skippedResourceNames, outTable);
                        }
                        if ((PSEUDO_BIDI & bundle->getPseudolocalize()) ==
                           PSEUDO_BIDI) {
                            block.setPosition(parserPosition);
                            err = parseAndAddEntry(bundle, in, &block, pseudoBidiParams,
                                    myPackage, curType, ident, *curTag, curIsStyled, curFormat,
                                    curIsFormatted, product,
                                    PSEUDO_BIDI, overwrite, &skippedResourceNames, outTable);
                        }
                        if (err != NO_ERROR) {
                            hasErrors = localHasErrors = true;
                        }
                    }
                }
            }

#if 0
            if (comment.size() > 0) {
                printf("Comment for @%s:%s/%s: %s\n", String8(myPackage).string(),
                       String8(curType).string(), String8(ident).string(),
                       String8(comment).string());
            }
#endif
            if (!localHasErrors) {
                outTable->appendComment(myPackage, curType, ident, comment, false);
            }
        }
        else if (code == ResXMLTree::END_TAG) {
            if (strcmp16(block.getElementName(&len), resources16.string()) != 0) {
                SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                        "Unexpected end tag %s\n", String8(block.getElementName(&len)).string());
                return UNKNOWN_ERROR;
            }
        }
        else if (code == ResXMLTree::START_NAMESPACE || code == ResXMLTree::END_NAMESPACE) {
        }
        else if (code == ResXMLTree::TEXT) {
            if (isWhitespace(block.getText(&len))) {
                continue;
            }
            SourcePos(in->getPrintableSource(), block.getLineNumber()).error(
                    "Found text \"%s\" where item tag is expected\n",
                    String8(block.getText(&len)).string());
            return UNKNOWN_ERROR;
        }
    }

    // For every resource defined, there must be exist one variant with a product attribute
    // set to 'default' (or no product attribute at all).
    // We check to see that for every resource that was ignored because of a mismatched
    // product attribute, some product variant of that resource was processed.
    for (size_t i = 0; i < skippedResourceNames.size(); i++) {
        if (skippedResourceNames[i]) {
            const type_ident_pair_t& p = skippedResourceNames.keyAt(i);
            if (!outTable->hasBagOrEntry(myPackage, p.type, p.ident)) {
                const char* bundleProduct =
                        (bundle->getProduct() == NULL) ? "" : bundle->getProduct();
                fprintf(stderr, "In resource file %s: %s\n",
                        in->getPrintableSource().string(),
                        curParams.toString().string());

                fprintf(stderr, "\t%s '%s' does not match product %s.\n"
                        "\tYou may have forgotten to include a 'default' product variant"
                        " of the resource.\n",
                        String8(p.type).string(), String8(p.ident).string(),
                        bundleProduct[0] == 0 ? "default" : bundleProduct);
                return UNKNOWN_ERROR;
            }
        }
    }

    return hasErrors ? UNKNOWN_ERROR : NO_ERROR;
}

ResourceTable::ResourceTable(Bundle* bundle, const String16& assetsPackage, ResourceTable::PackageType type)
    : mAssetsPackage(assetsPackage)
    , mPackageType(type)
    , mTypeIdOffset(0)
    , mNumLocal(0)
    , mBundle(bundle)
{
   
    
    ssize_t packageId = -1;
    
    switch (mPackageType) {
        case App:
        case AppFeature:
            packageId = 0x7f;
            break;
        case System:
            packageId = 0x01;
            break;
        case SharedLibrary:
            packageId = 0x00;
            break;
            
//        case Voice:
//            packageId = 0x34;
//            break;
//        case Call:
//            packageId = 0x35;
//            break;
//        case Search:
//            packageId = 0x36;
//            break;
//        case Schedule:
//            packageId = 0x37;
//            break;
//        case Train:
//            packageId = 0x38;
//            break;
//        case Destination:
//            packageId = 0x44;
//            break;
//        case Chat:
//            packageId = 0x46;
//            break;
//        case Flight:
//            packageId = 0x52;
//            break;
//        case MyCtrip:
//            packageId = 0x54;
//            break;
//        case Pay:
//            packageId = 0x55;
//            break;
//        case Foundation:
//            packageId = 0x56;
//            break;
//        case Hotel:
//            packageId = 0x58;
//            break;
//        case Container:
//            packageId = 0x61;
//            break;
//        case CustomerService:
//            packageId = 0x62;
//            break;
//        case ThirdParty:
//            packageId = 0x63;
//            break;
//        case Extend1:
//            packageId = 0x64;
//            break;
//        case Extend2:
//            packageId = 0x65;
//            break;
//        case Extend3:
//            packageId = 0x66;
//            break;
//        case Extend4:
//            packageId = 0x67;
//            break;
//        case Extend5:
//            packageId = 0x68;
//            break;
//        case Extend6:
//            packageId = 0x69;
//            break;
        default:
            assert(0);
            break;
    }
    if(!bundle->getApkModule().isEmpty()){
        android::String8 apkmoduleVal=bundle->getApkModule();
        packageId=apkStringToInt(apkmoduleVal);
    }
    
    sp<Package> package = new Package(mAssetsPackage, packageId);
    mPackages.add(assetsPackage, package);
    mOrderedPackages.add(package);

    // Every resource table always has one first entry, the bag attributes.
    const SourcePos unknown(String8("????"), 0);
    getType(mAssetsPackage, String16("attr"), unknown);
}
ssize_t ResourceTable::apkStringToInt(const String8& s){
    size_t i = 0;
    ssize_t val = 0;
    size_t len=s.length();
    if (s[i] < '0' || s[i] > '9') {
        return -1;
    }
    
    // Decimal or hex?
    if (s[i] == '0' && s[i+1] == 'x') {
        i += 2;
        bool error = false;
        while (i < len && !error) {
            val = (val*16) + apkgetHex(s[i], &error);
            i++;
        }
        if (error) {
            return -1;
        }
    } else {
        while (i < len) {
            if (s[i] < '0' || s[i] > '9') {
                return false;
            }
            val = (val*10) + s[i]-'0';
            i++;
        }
    }
    
    if (i == len) {
        return val;
    }
    return -1;
}
uint32_t ResourceTable::apkgetHex(char c, bool* outError){
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 0xa;
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 0xa;
    }
    *outError = true;
    return 0;
}

static uint32_t findLargestTypeIdForPackage(const ResTable& table, const String16& packageName) {
    const size_t basePackageCount = table.getBasePackageCount();
    for (size_t i = 0; i < basePackageCount; i++) {
        if (packageName == table.getBasePackageName(i)) {
            return table.getLastTypeIdForPackage(i);
        }
    }
    return 0;
}

status_t ResourceTable::addIncludedResources(Bundle* bundle, const sp<AaptAssets>& assets)
{
    status_t err = assets->buildIncludedResources(bundle);
    if (err != NO_ERROR) {
        return err;
    }

    mAssets = assets;
    mTypeIdOffset = findLargestTypeIdForPackage(assets->getIncludedResources(), mAssetsPackage);

    const String8& featureAfter = bundle->getFeatureAfterPackage();
    if (!featureAfter.isEmpty()) {
        AssetManager featureAssetManager;
        if (!featureAssetManager.addAssetPath(featureAfter, NULL)) {
            fprintf(stderr, "ERROR: Feature package '%s' not found.\n",
                    featureAfter.string());
            return UNKNOWN_ERROR;
        }

        const ResTable& featureTable = featureAssetManager.getResources(false);
        mTypeIdOffset = max(mTypeIdOffset,
                findLargestTypeIdForPackage(featureTable, mAssetsPackage)); 
    }

    return NO_ERROR;
}

status_t ResourceTable::addPublic(const SourcePos& sourcePos,
                                  const String16& package,
                                  const String16& type,
                                  const String16& name,
                                  const uint32_t ident)
{
    uint32_t rid = mAssets->getIncludedResources()
        .identifierForName(name.string(), name.size(),
                           type.string(), type.size(),
                           package.string(), package.size());
    if (rid != 0) {
        sourcePos.error("Error declaring public resource %s/%s for included package %s\n",
                String8(type).string(), String8(name).string(),
                String8(package).string());
        return UNKNOWN_ERROR;
    }

    sp<Type> t = getType(package, type, sourcePos);
    if (t == NULL) {
        return UNKNOWN_ERROR;
    }
    return t->addPublic(sourcePos, name, ident);
}

status_t ResourceTable::addEntry(const SourcePos& sourcePos,
                                 const String16& package,
                                 const String16& type,
                                 const String16& name,
                                 const String16& value,
                                 const Vector<StringPool::entry_style_span>* style,
                                 const ResTable_config* params,
                                 const bool doSetIndex,
                                 const int32_t format,
                                 const bool overwrite)
{
    uint32_t rid = mAssets->getIncludedResources()
        .identifierForName(name.string(), name.size(),
                           type.string(), type.size(),
                           package.string(), package.size());
    if (rid != 0) {
        sourcePos.error("Resource entry %s/%s is already defined in package %s.",
                String8(type).string(), String8(name).string(), String8(package).string());
        return UNKNOWN_ERROR;
    }
    
    sp<Entry> e = getEntry(package, type, name, sourcePos, overwrite,
                           params, doSetIndex);
    if (e == NULL) {
        return UNKNOWN_ERROR;
    }
    status_t err = e->setItem(sourcePos, value, style, format, overwrite);
    if (err == NO_ERROR) {
        mNumLocal++;
    }
    return err;
}

status_t ResourceTable::startBag(const SourcePos& sourcePos,
                                 const String16& package,
                                 const String16& type,
                                 const String16& name,
                                 const String16& bagParent,
                                 const ResTable_config* params,
                                 bool overlay,
                                 bool replace, bool isId)
{
    status_t result = NO_ERROR;

    // Check for adding entries in other packages...  for now we do
    // nothing.  We need to do the right thing here to support skinning.
    uint32_t rid = mAssets->getIncludedResources()
    .identifierForName(name.string(), name.size(),
                       type.string(), type.size(),
                       package.string(), package.size());
    if (rid != 0) {
        sourcePos.error("Resource entry %s/%s is already defined in package %s.",
                String8(type).string(), String8(name).string(), String8(package).string());
        return UNKNOWN_ERROR;
    }

    if (overlay && !mBundle->getAutoAddOverlay() && !hasBagOrEntry(package, type, name)) {
        bool canAdd = false;
        sp<Package> p = mPackages.valueFor(package);
        if (p != NULL) {
            sp<Type> t = p->getTypes().valueFor(type);
            if (t != NULL) {
                if (t->getCanAddEntries().indexOf(name) >= 0) {
                    canAdd = true;
                }
            }
        }
        if (!canAdd) {
            sourcePos.error("Resource does not already exist in overlay at '%s'; use <add-resource> to add.\n",
                            String8(name).string());
            return UNKNOWN_ERROR;
        }
    }
    sp<Entry> e = getEntry(package, type, name, sourcePos, overlay, params);
    if (e == NULL) {
        return UNKNOWN_ERROR;
    }
    
    // If a parent is explicitly specified, set it.
    if (bagParent.size() > 0) {
        e->setParent(bagParent);
    }

    if ((result = e->makeItABag(sourcePos)) != NO_ERROR) {
        return result;
    }

    if (overlay && replace) { 
        return e->emptyBag(sourcePos);
    }
    return result;
}

status_t ResourceTable::addBag(const SourcePos& sourcePos,
                               const String16& package,
                               const String16& type,
                               const String16& name,
                               const String16& bagParent,
                               const String16& bagKey,
                               const String16& value,
                               const Vector<StringPool::entry_style_span>* style,
                               const ResTable_config* params,
                               bool replace, bool isId, const int32_t format)
{
    // Check for adding entries in other packages...  for now we do
    // nothing.  We need to do the right thing here to support skinning.
    uint32_t rid = mAssets->getIncludedResources()
        .identifierForName(name.string(), name.size(),
                           type.string(), type.size(),
                           package.string(), package.size());
    if (rid != 0) {
        return NO_ERROR;
    }

#if 0
    if (name == String16("left")) {
        printf("Adding bag left: file=%s, line=%d, type=%s\n",
               sourcePos.file.striing(), sourcePos.line, String8(type).string());
    }
#endif
    sp<Entry> e = getEntry(package, type, name, sourcePos, replace, params);
    if (e == NULL) {
        return UNKNOWN_ERROR;
    }

    // If a parent is explicitly specified, set it.
    if (bagParent.size() > 0) {
        e->setParent(bagParent);
    }

    const bool first = e->getBag().indexOfKey(bagKey) < 0;
    status_t err = e->addToBag(sourcePos, bagKey, value, style, replace, isId, format);
    if (err == NO_ERROR && first) {
        mNumLocal++;
    }
    return err;
}

bool ResourceTable::hasBagOrEntry(const String16& package,
                                  const String16& type,
                                  const String16& name) const
{
    // First look for this in the included resources...
    uint32_t rid = mAssets->getIncludedResources()
        .identifierForName(name.string(), name.size(),
                           type.string(), type.size(),
                           package.string(), package.size());
    if (rid != 0) {
        return true;
    }

    sp<Package> p = mPackages.valueFor(package);
    if (p != NULL) {
        sp<Type> t = p->getTypes().valueFor(type);
        if (t != NULL) {
            sp<ConfigList> c =  t->getConfigs().valueFor(name);
            if (c != NULL) return true;
        }
    }

    return false;
}

bool ResourceTable::hasBagOrEntry(const String16& package,
                                  const String16& type,
                                  const String16& name,
                                  const ResTable_config& config) const
{
    // First look for this in the included resources...
    uint32_t rid = mAssets->getIncludedResources()
        .identifierForName(name.string(), name.size(),
                           type.string(), type.size(),
                           package.string(), package.size());
    if (rid != 0) {
        return true;
    }

    sp<Package> p = mPackages.valueFor(package);
    if (p != NULL) {
        sp<Type> t = p->getTypes().valueFor(type);
        if (t != NULL) {
            sp<ConfigList> c =  t->getConfigs().valueFor(name);
            if (c != NULL) {
                sp<Entry> e = c->getEntries().valueFor(config);
                if (e != NULL) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool ResourceTable::hasBagOrEntry(const String16& ref,
                                  const String16* defType,
                                  const String16* defPackage)
{
    String16 package, type, name;
    if (!ResTable::expandResourceRef(ref.string(), ref.size(), &package, &type, &name,
                defType, defPackage ? defPackage:&mAssetsPackage, NULL)) {
        return false;
    }
    return hasBagOrEntry(package, type, name);
}

bool ResourceTable::appendComment(const String16& package,
                                  const String16& type,
                                  const String16& name,
                                  const String16& comment,
                                  bool onlyIfEmpty)
{
    if (comment.size() <= 0) {
        return true;
    }

    sp<Package> p = mPackages.valueFor(package);
    if (p != NULL) {
        sp<Type> t = p->getTypes().valueFor(type);
        if (t != NULL) {
            sp<ConfigList> c =  t->getConfigs().valueFor(name);
            if (c != NULL) {
                c->appendComment(comment, onlyIfEmpty);
                return true;
            }
        }
    }
    return false;
}

bool ResourceTable::appendTypeComment(const String16& package,
                                      const String16& type,
                                      const String16& name,
                                      const String16& comment)
{
    if (comment.size() <= 0) {
        return true;
    }
    
    sp<Package> p = mPackages.valueFor(package);
    if (p != NULL) {
        sp<Type> t = p->getTypes().valueFor(type);
        if (t != NULL) {
            sp<ConfigList> c =  t->getConfigs().valueFor(name);
            if (c != NULL) {
                c->appendTypeComment(comment);
                return true;
            }
        }
    }
    return false;
}

void ResourceTable::canAddEntry(const SourcePos& pos,
        const String16& package, const String16& type, const String16& name)
{
    sp<Type> t = getType(package, type, pos);
    if (t != NULL) {
        t->canAddEntry(name);
    }
}

size_t ResourceTable::size() const {
    return mPackages.size();
}

size_t ResourceTable::numLocalResources() const {
    return mNumLocal;
}

bool ResourceTable::hasResources() const {
    return mNumLocal > 0;
}

sp<AaptFile> ResourceTable::flatten(Bundle* bundle, const sp<const ResourceFilter>& filter,
        const bool isBase)
{
    sp<AaptFile> data = new AaptFile(String8(), AaptGroupEntry(), String8());
    status_t err = flatten(bundle, filter, data, isBase);
    return err == NO_ERROR ? data : NULL;
}

inline uint32_t ResourceTable::getResId(const sp<Package>& p,
                                        const sp<Type>& t,
                                        uint32_t nameId)
{
    return makeResId(p->getAssignedId(), t->getIndex(), nameId);
}

uint32_t ResourceTable::getResId(const String16& package,
                                 const String16& type,
                                 const String16& name,
                                 bool onlyPublic) const
{
    uint32_t id = ResourceIdCache::lookup(package, type, name, onlyPublic);
    if (id != 0) return id;     // cache hit

    // First look for this in the included resources...
    uint32_t specFlags = 0;
    uint32_t rid = mAssets->getIncludedResources()
        .identifierForName(name.string(), name.size(),
                           type.string(), type.size(),
                           package.string(), package.size(),
                           &specFlags);
    if (rid != 0) {
        if (onlyPublic) {
            if ((specFlags & ResTable_typeSpec::SPEC_PUBLIC) == 0) {
//                printf("************** RID=0x%08x,[%s:%s:%s] is not 00000, but only Public********\n", name.string(), type.string(), package.string());
                return 0;
            }
        }
        
        return ResourceIdCache::store(package, type, name, onlyPublic, rid);
    }
    sp<Package> p = mPackages.valueFor(package);
    if (p == NULL){
        fprintf(stderr,"Package is NULL\n");
        return 0;
    }
    sp<Type> t = p->getTypes().valueFor(type);
    if (t == NULL) {
        fprintf(stderr,"Type is NULL\n");
        return 0;
    }
    sp<ConfigList> c =  t->getConfigs().valueFor(name);
    if (c == NULL) {
        fprintf(stderr,"ConfigList is NULL\n");
        return 0;
    }
    int32_t ei = c->getEntryIndex();
    if (ei < 0) {
        fprintf(stderr,"EntryIndex is NULL\n");
        return 0;
    }

    return ResourceIdCache::store(package, type, name, onlyPublic,
            getResId(p, t, ei));
}

uint32_t ResourceTable::getResId(const String16& ref,
                                 const String16* defType,
                                 const String16* defPackage,
                                 const char** outErrorMsg,
                                 bool onlyPublic) const
{
    String16 package, type, name;
    bool refOnlyPublic = true;
    if (!ResTable::expandResourceRef(
        ref.string(), ref.size(), &package, &type, &name,
        defType, defPackage ? defPackage:&mAssetsPackage,
        outErrorMsg, &refOnlyPublic)) {
        NOISY(printf("Expanding resource: ref=%s\n",
                     String8(ref).string()));
        NOISY(printf("Expanding resource: defType=%s\n",
                     defType ? String8(*defType).string() : "NULL"));
        NOISY(printf("Expanding resource: defPackage=%s\n",
                     defPackage ? String8(*defPackage).string() : "NULL"));
        NOISY(printf("Expanding resource: ref=%s\n", String8(ref).string()));
        NOISY(printf("Expanded resource: p=%s, t=%s, n=%s, res=0\n",
                     String8(package).string(), String8(type).string(),
                     String8(name).string()));
        return 0;
    }
    uint32_t res = getResId(package, type, name, onlyPublic && refOnlyPublic);
    NOISY(printf("Expanded resource: p=%s, t=%s, n=%s, res=%d\n",
                 String8(package).string(), String8(type).string(),
                 String8(name).string(), res));
    if (res == 0) {
        if (outErrorMsg)
            *outErrorMsg = "No resource found that matches the given name";
    }
    return res;
}

bool ResourceTable::isValidResourceName(const String16& s)
{
    const char16_t* p = s.string();
    bool first = true;
    while (*p) {
        if ((*p >= 'a' && *p <= 'z')
            || (*p >= 'A' && *p <= 'Z')
            || *p == '_'
            || (!first && *p >= '0' && *p <= '9')) {
            first = false;
            p++;
            continue;
        }
        return false;
    }
    return true;
}

bool ResourceTable::stringToValue(Res_value* outValue,
                                  StringPool* pool,
                                  const String16& str,
                                  bool preserveSpaces, bool coerceType,
                                  uint32_t attrID,
                                  const Vector<StringPool::entry_style_span>* style,
                                  String16* outStr, void* accessorCookie,
                                  uint32_t attrType, const String8* configTypeName,
                                  const ConfigDescription* config)

{
    String16 finalStr;

    bool res = true;
    if (style == NULL || style->size() == 0) {
        // Text is not styled so it can be any type...  let's figure it out.
        res = mAssets->getIncludedResources()
            .stringToValue(outValue, &finalStr, str.string(), str.size(), preserveSpaces,
                            coerceType, attrID, NULL, &mAssetsPackage, this,
                           accessorCookie, attrType);
    } else {
        // Styled text can only be a string, and while collecting the style
        // information we have already processed that string!
        outValue->size = sizeof(Res_value);
        outValue->res0 = 0;
        outValue->dataType = outValue->TYPE_STRING;
        outValue->data = 0;
        finalStr = str;
    }

    if (!res) {
        fprintf(stderr,"ResourceTable::stringToValue\n");
        return false;
    }

    if (outValue->dataType == outValue->TYPE_STRING) {
        // Should do better merging styles.
        if (pool) {
            String8 configStr;
            if (config != NULL) {
                configStr = config->toString();
            } else {
                configStr = "(null)";
            }
            NOISY(printf("Adding to pool string style #%d config %s: %s\n",
                    style != NULL ? style->size() : 0,
                    configStr.string(), String8(finalStr).string()));
            if (style != NULL && style->size() > 0) {
                outValue->data = pool->add(finalStr, *style, configTypeName, config);
            } else {
                outValue->data = pool->add(finalStr, true, configTypeName, config);
            }
        } else {
            // Caller will fill this in later.
            outValue->data = 0;
        }
    
        if (outStr) {
            *outStr = finalStr;
        }

    }

    return true;
}

uint32_t ResourceTable::getCustomResource(
    const String16& package, const String16& type, const String16& name) const
{
    //printf("getCustomResource: %s %s %s\n", String8(package).string(),
    //       String8(type).string(), String8(name).string());
    sp<Package> p = mPackages.valueFor(package);
    if (p == NULL) return 0;
    sp<Type> t = p->getTypes().valueFor(type);
    if (t == NULL) return 0;
    sp<ConfigList> c =  t->getConfigs().valueFor(name);
    if (c == NULL) return 0;
    int32_t ei = c->getEntryIndex();
    if (ei < 0) return 0;
    return getResId(p, t, ei);
}

uint32_t ResourceTable::getCustomResourceWithCreation(
        const String16& package, const String16& type, const String16& name,
        const bool createIfNotFound)
{
    uint32_t resId = getCustomResource(package, type, name);
    if (resId != 0 || !createIfNotFound) {
        return resId;
    }

    if (mAssetsPackage != package) {
        mCurrentXmlPos.error("creating resource for external package %s: %s/%s.",
                String8(package).string(), String8(type).string(), String8(name).string());
        if (package == String16("android")) {
            mCurrentXmlPos.printf("did you mean to use @+id instead of @+android:id?");
        }
        return 0;
    }

    String16 value("false");
    status_t status = addEntry(mCurrentXmlPos, package, type, name, value, NULL, NULL, true);
    if (status == NO_ERROR) {
        resId = getResId(package, type, name);
        return resId;
    }
    return 0;
}

uint32_t ResourceTable::getRemappedPackage(uint32_t origPackage) const
{
    return origPackage;
}

bool ResourceTable::getAttributeType(uint32_t attrID, uint32_t* outType)
{
    //printf("getAttributeType #%08x\n", attrID);
    Res_value value;
    if (getItemValue(attrID, ResTable_map::ATTR_TYPE, &value)) {
        //printf("getAttributeType #%08x (%s): #%08x\n", attrID,
        //       String8(getEntry(attrID)->getName()).string(), value.data);
        *outType = value.data;
        return true;
    }
    return false;
}

bool ResourceTable::getAttributeMin(uint32_t attrID, uint32_t* outMin)
{
    //printf("getAttributeMin #%08x\n", attrID);
    Res_value value;
    if (getItemValue(attrID, ResTable_map::ATTR_MIN, &value)) {
        *outMin = value.data;
        return true;
    }
    return false;
}

bool ResourceTable::getAttributeMax(uint32_t attrID, uint32_t* outMax)
{
    //printf("getAttributeMax #%08x\n", attrID);
    Res_value value;
    if (getItemValue(attrID, ResTable_map::ATTR_MAX, &value)) {
        *outMax = value.data;
        return true;
    }
    return false;
}

uint32_t ResourceTable::getAttributeL10N(uint32_t attrID)
{
    //printf("getAttributeL10N #%08x\n", attrID);
    Res_value value;
    if (getItemValue(attrID, ResTable_map::ATTR_L10N, &value)) {
        return value.data;
    }
    return ResTable_map::L10N_NOT_REQUIRED;
}

bool ResourceTable::getLocalizationSetting()
{
    return mBundle->getRequireLocalization();
}

void ResourceTable::reportError(void* accessorCookie, const char* fmt, ...)
{
    if (accessorCookie != NULL && fmt != NULL) {
        AccessorCookie* ac = (AccessorCookie*)accessorCookie;
        int retval=0;
        char buf[1024];
        va_list ap;
        va_start(ap, fmt);
        retval = vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        ac->sourcePos.error("Error: %s (at '%s' with value '%s').\n",
                            buf, ac->attr.string(), ac->value.string());
    }
}

bool ResourceTable::getAttributeKeys(
    uint32_t attrID, Vector<String16>* outKeys)
{
    sp<const Entry> e = getEntry(attrID);
    if (e != NULL) {
        const size_t N = e->getBag().size();
        for (size_t i=0; i<N; i++) {
            const String16& key = e->getBag().keyAt(i);
            if (key.size() > 0 && key.string()[0] != '^') {
                outKeys->add(key);
            }
        }
        return true;
    }
    return false;
}

bool ResourceTable::getAttributeEnum(
    uint32_t attrID, const char16_t* name, size_t nameLen,
    Res_value* outValue)
{
    //printf("getAttributeEnum #%08x %s\n", attrID, String8(name, nameLen).string());
    String16 nameStr(name, nameLen);
    sp<const Entry> e = getEntry(attrID);
    if (e != NULL) {
        const size_t N = e->getBag().size();
        for (size_t i=0; i<N; i++) {
            //printf("Comparing %s to %s\n", String8(name, nameLen).string(),
            //       String8(e->getBag().keyAt(i)).string());
            if (e->getBag().keyAt(i) == nameStr) {
                return getItemValue(attrID, e->getBag().valueAt(i).bagKeyId, outValue);
            }
        }
    }
    return false;
}

bool ResourceTable::getAttributeFlags(
    uint32_t attrID, const char16_t* name, size_t nameLen,
    Res_value* outValue)
{
    outValue->dataType = Res_value::TYPE_INT_HEX;
    outValue->data = 0;

    //printf("getAttributeFlags #%08x %s\n", attrID, String8(name, nameLen).string());
    String16 nameStr(name, nameLen);
    sp<const Entry> e = getEntry(attrID);
    if (e != NULL) {
        const size_t N = e->getBag().size();

        const char16_t* end = name + nameLen;
        const char16_t* pos = name;
        while (pos < end) {
            const char16_t* start = pos;
            while (pos < end && *pos != '|') {
                pos++;
            }

            String16 nameStr(start, pos-start);
            size_t i;
            for (i=0; i<N; i++) {
                //printf("Comparing \"%s\" to \"%s\"\n", String8(nameStr).string(),
                //       String8(e->getBag().keyAt(i)).string());
                if (e->getBag().keyAt(i) == nameStr) {
                    Res_value val;
                    bool got = getItemValue(attrID, e->getBag().valueAt(i).bagKeyId, &val);
                    if (!got) {
                        return false;
                    }
                    //printf("Got value: 0x%08x\n", val.data);
                    outValue->data |= val.data;
                    break;
                }
            }

            if (i >= N) {
                // Didn't find this flag identifier.
                return false;
            }
            pos++;
        }

        return true;
    }
    return false;
}

status_t ResourceTable::assignResourceIds()
{
    const size_t N = mOrderedPackages.size();
    size_t pi;
    status_t firstError = NO_ERROR;

    // First generate all bag attributes and assign indices.
    for (pi=0; pi<N; pi++) {
        sp<Package> p = mOrderedPackages.itemAt(pi);
        if (p == NULL || p->getTypes().size() == 0) {
            // Empty, skip!
            continue;
        }

        // This has no sense for packages being built as AppFeature (aka with a non-zero offset).
        status_t err = p->applyPublicTypeOrder();
        if (err != NO_ERROR && firstError == NO_ERROR) {
            firstError = err;
        }

        // Generate attributes...
        const size_t N = p->getOrderedTypes().size();
        size_t ti;
        for (ti=0; ti<N; ti++) {
            sp<Type> t = p->getOrderedTypes().itemAt(ti);
            if (t == NULL) {
                continue;
            }
            const size_t N = t->getOrderedConfigs().size();
            for (size_t ci=0; ci<N; ci++) {
                sp<ConfigList> c = t->getOrderedConfigs().itemAt(ci);
                if (c == NULL) {
                    continue;
                }
                const size_t N = c->getEntries().size();
                for (size_t ei=0; ei<N; ei++) {
                    sp<Entry> e = c->getEntries().valueAt(ei);
                    if (e == NULL) {
                        continue;
                    }
                    status_t err = e->generateAttributes(this, p->getName());
                    if (err != NO_ERROR && firstError == NO_ERROR) {
                        firstError = err;
                    }
                }
            }
        }

        uint32_t typeIdOffset = 0;
        if (mPackageType == AppFeature && p->getName() == mAssetsPackage) {
            typeIdOffset = mTypeIdOffset;
        }

        const SourcePos unknown(String8("????"), 0);
        sp<Type> attr = p->getType(String16("attr"), unknown);

        // Assign indices...
        const size_t typeCount = p->getOrderedTypes().size();
        for (size_t ti = 0; ti < typeCount; ti++) {
            sp<Type> t = p->getOrderedTypes().itemAt(ti);
            if (t == NULL) {
                continue;
            }

            err = t->applyPublicEntryOrder();
            if (err != NO_ERROR && firstError == NO_ERROR) {
                firstError = err;
            }

            const size_t N = t->getOrderedConfigs().size();
            t->setIndex(ti + 1 + typeIdOffset);

            LOG_ALWAYS_FATAL_IF(ti == 0 && attr != t,
                                "First type is not attr!");

            for (size_t ei=0; ei<N; ei++) {
                sp<ConfigList> c = t->getOrderedConfigs().itemAt(ei);
                if (c == NULL) {
                    continue;
                }
                c->setEntryIndex(ei);
            }
        }

        // Assign resource IDs to keys in bags...
        for (size_t ti = 0; ti < typeCount; ti++) {
            sp<Type> t = p->getOrderedTypes().itemAt(ti);
            if (t == NULL) {
                continue;
            }
            const size_t N = t->getOrderedConfigs().size();
            for (size_t ci=0; ci<N; ci++) {
                sp<ConfigList> c = t->getOrderedConfigs().itemAt(ci);
                //printf("Ordered config #%d: %p\n", ci, c.get());
                const size_t N = c->getEntries().size();
                for (size_t ei=0; ei<N; ei++) {
                    sp<Entry> e = c->getEntries().valueAt(ei);
                    if (e == NULL) {
                        continue;
                    }
                    status_t err = e->assignResourceIds(this, p->getName());
                    if (err != NO_ERROR && firstError == NO_ERROR) {
                        firstError = err;
                    }
                }
            }
        }
    }
    return firstError;
}

status_t ResourceTable::addSymbols(const sp<AaptSymbols>& outSymbols) {
    const size_t N = mOrderedPackages.size();
    size_t pi;

    for (pi=0; pi<N; pi++) {
        sp<Package> p = mOrderedPackages.itemAt(pi);
        if (p->getTypes().size() == 0) {
            // Empty, skip!
            continue;
        }

        const size_t N = p->getOrderedTypes().size();
        size_t ti;

        for (ti=0; ti<N; ti++) {
            sp<Type> t = p->getOrderedTypes().itemAt(ti);
            if (t == NULL) {
                continue;
            }
            const size_t N = t->getOrderedConfigs().size();
            sp<AaptSymbols> typeSymbols =
                    outSymbols->addNestedSymbol(String8(t->getName()), t->getPos());
            if (typeSymbols == NULL) {
                return UNKNOWN_ERROR;
            }

            for (size_t ci=0; ci<N; ci++) {
                sp<ConfigList> c = t->getOrderedConfigs().itemAt(ci);
                if (c == NULL) {
                    continue;
                }
                uint32_t rid = getResId(p, t, ci);
                if (rid == 0) {
                    return UNKNOWN_ERROR;
                }
                if (Res_GETPACKAGE(rid) + 1 == p->getAssignedId()) {
                    typeSymbols->addSymbol(String8(c->getName()), rid, c->getPos());
                    
                    String16 comment(c->getComment());
                    typeSymbols->appendComment(String8(c->getName()), comment, c->getPos());
                    //printf("Type symbol [%08x] %s comment: %s\n", rid,
                    //        String8(c->getName()).string(), String8(comment).string());
                    comment = c->getTypeComment();
                    typeSymbols->appendTypeComment(String8(c->getName()), comment);
                }
            }
        }
    }
    return NO_ERROR;
}


void
ResourceTable::addLocalization(const String16& name, const String8& locale, const SourcePos& src)
{
    mLocalizations[name][locale] = src;
}


/*!
 * Flag various sorts of localization problems.  '+' indicates checks already implemented;
 * '-' indicates checks that will be implemented in the future.
 *
 * + A localized string for which no default-locale version exists => warning
 * + A string for which no version in an explicitly-requested locale exists => warning
 * + A localized translation of an translateable="false" string => warning
 * - A localized string not provided in every locale used by the table
 */
status_t
ResourceTable::validateLocalizations(void)
{
    status_t err = NO_ERROR;
    const String8 defaultLocale;

    // For all strings...
    for (map<String16, map<String8, SourcePos> >::iterator nameIter = mLocalizations.begin();
         nameIter != mLocalizations.end();
         nameIter++) {
        const map<String8, SourcePos>& configSrcMap = nameIter->second;

        // Look for strings with no default localization
        if (configSrcMap.count(defaultLocale) == 0) {
            SourcePos().warning("string '%s' has no default translation.",
                    String8(nameIter->first).string());
            if (mBundle->getVerbose()) {
                for (map<String8, SourcePos>::const_iterator locales = configSrcMap.begin();
                    locales != configSrcMap.end();
                    locales++) {
                    locales->second.printf("locale %s found", locales->first.string());
                }
            }
            // !!! TODO: throw an error here in some circumstances
        }

        // Check that all requested localizations are present for this string
        if (mBundle->getConfigurations().size() > 0 && mBundle->getRequireLocalization()) {
            const char* allConfigs = mBundle->getConfigurations().string();
            const char* start = allConfigs;
            const char* comma;
            
            set<String8> missingConfigs;
            AaptLocaleValue locale;
            do {
                String8 config;
                comma = strchr(start, ',');
                if (comma != NULL) {
                    config.setTo(start, comma - start);
                    start = comma + 1;
                } else {
                    config.setTo(start);
                }

                if (!locale.initFromFilterString(config)) {
                    continue;
                }

                // don't bother with the pseudolocale "en_XA" or "ar_XB"
                if (config != "en_XA" && config != "ar_XB") {
                    if (configSrcMap.find(config) == configSrcMap.end()) {
                        // okay, no specific localization found.  it's possible that we are
                        // requiring a specific regional localization [e.g. de_DE] but there is an
                        // available string in the generic language localization [e.g. de];
                        // consider that string to have fulfilled the localization requirement.
                        String8 region(config.string(), 2);
                        if (configSrcMap.find(region) == configSrcMap.end() &&
                                configSrcMap.count(defaultLocale) == 0) {
                            missingConfigs.insert(config);
                        }
                    }
                }
            } while (comma != NULL);

            if (!missingConfigs.empty()) {
                String8 configStr;
                for (set<String8>::iterator iter = missingConfigs.begin();
                     iter != missingConfigs.end();
                     iter++) {
                    configStr.appendFormat(" %s", iter->string());
                }
                SourcePos().warning("string '%s' is missing %u required localizations:%s",
                        String8(nameIter->first).string(),
                        (unsigned int)missingConfigs.size(),
                        configStr.string());
            }
        }
    }

    return err;
}

status_t ResourceTable::flatten(Bundle* bundle, const sp<const ResourceFilter>& filter,
        const sp<AaptFile>& dest,
        const bool isBase)
{
    const ConfigDescription nullConfig;

    const size_t N = mOrderedPackages.size();
    size_t pi;

    const static String16 mipmap16("mipmap");

    bool useUTF8 = !bundle->getUTF16StringsOption();

    // The libraries this table references.
    Vector<sp<Package> > libraryPackages;
    const ResTable& table = mAssets->getIncludedResources();
    const size_t basePackageCount = table.getBasePackageCount();
    for (size_t i = 0; i < basePackageCount; i++) {
        size_t packageId = table.getBasePackageId(i);
        String16 packageName(table.getBasePackageName(i));
        if (packageId > 0x01 && packageId != 0x7f &&
                packageName != String16("android")) {
            libraryPackages.add(sp<Package>(new Package(packageName, packageId)));
        }
    }

    // Iterate through all data, collecting all values (strings,
    // references, etc).
    StringPool valueStrings(useUTF8);
    Vector<sp<Entry> > allEntries;
    for (pi=0; pi<N; pi++) {
        sp<Package> p = mOrderedPackages.itemAt(pi);
        if (p->getTypes().size() == 0) {
            continue;
        }

        StringPool typeStrings(useUTF8);
        StringPool keyStrings(useUTF8);

        ssize_t stringsAdded = 0;
        const size_t N = p->getOrderedTypes().size();
        for (size_t ti=0; ti<N; ti++) {
            sp<Type> t = p->getOrderedTypes().itemAt(ti);
            if (t == NULL) {
                typeStrings.add(String16("<empty>"), false);
                stringsAdded++;
                continue;
            }

            while (stringsAdded < t->getIndex() - 1) {
                typeStrings.add(String16("<empty>"), false);
                stringsAdded++;
            }

            const String16 typeName(t->getName());
            typeStrings.add(typeName, false);
            stringsAdded++;

            // This is a hack to tweak the sorting order of the final strings,
            // to put stuff that is generally not language-specific first.
            String8 configTypeName(typeName);
            if (configTypeName == "drawable" || configTypeName == "layout"
                    || configTypeName == "color" || configTypeName == "anim"
                    || configTypeName == "interpolator" || configTypeName == "animator"
                    || configTypeName == "xml" || configTypeName == "menu"
                    || configTypeName == "mipmap" || configTypeName == "raw") {
                configTypeName = "1complex";
            } else {
                configTypeName = "2value";
            }

            // mipmaps don't get filtered, so they will
            // allways end up in the base. Make sure they
            // don't end up in a split.
            if (typeName == mipmap16 && !isBase) {
                continue;
            }

            const bool filterable = (typeName != mipmap16);

            const size_t N = t->getOrderedConfigs().size();
            for (size_t ci=0; ci<N; ci++) {
                sp<ConfigList> c = t->getOrderedConfigs().itemAt(ci);
                if (c == NULL) {
                    continue;
                }
                const size_t N = c->getEntries().size();
                for (size_t ei=0; ei<N; ei++) {
                    ConfigDescription config = c->getEntries().keyAt(ei);
                    if (filterable && !filter->match(config)) {
                        continue;
                    }
                    sp<Entry> e = c->getEntries().valueAt(ei);
                    if (e == NULL) {
                        continue;
                    }
                    e->setNameIndex(keyStrings.add(e->getName(), true));

                    // If this entry has no values for other configs,
                    // and is the default config, then it is special.  Otherwise
                    // we want to add it with the config info.
                    ConfigDescription* valueConfig = NULL;
                    if (N != 1 || config == nullConfig) {
                        valueConfig = &config;
                    }

                    status_t err = e->prepareFlatten(&valueStrings, this,
                            &configTypeName, &config);
                    if (err != NO_ERROR) {
                        return err;
                    }
                    allEntries.add(e);
                }
            }
        }

        p->setTypeStrings(typeStrings.createStringBlock());
        p->setKeyStrings(keyStrings.createStringBlock());
    }

    if (bundle->getOutputAPKFile() != NULL) {
        // Now we want to sort the value strings for better locality.  This will
        // cause the positions of the strings to change, so we need to go back
        // through out resource entries and update them accordingly.  Only need
        // to do this if actually writing the output file.
        valueStrings.sortByConfig();
        for (pi=0; pi<allEntries.size(); pi++) {
            allEntries[pi]->remapStringValue(&valueStrings);
        }
    }

    ssize_t strAmt = 0;

    // Now build the array of package chunks.
    Vector<sp<AaptFile> > flatPackages;
    for (pi=0; pi<N; pi++) {
        sp<Package> p = mOrderedPackages.itemAt(pi);
        if (p->getTypes().size() == 0) {
            // Empty, skip!
            continue;
        }

        const size_t N = p->getTypeStrings().size();

        const size_t baseSize = sizeof(ResTable_package);

        // Start the package data.
        sp<AaptFile> data = new AaptFile(String8(), AaptGroupEntry(), String8());
        ResTable_package* header = (ResTable_package*)data->editData(baseSize);
        if (header == NULL) {
            fprintf(stderr, "ERROR: out of memory creating ResTable_package\n");
            return NO_MEMORY;
        }
        memset(header, 0, sizeof(*header));
        header->header.type = htods(RES_TABLE_PACKAGE_TYPE);
        header->header.headerSize = htods(sizeof(*header));
        header->id = htodl(static_cast<uint32_t>(p->getAssignedId()));
        strcpy16_htod(header->name, p->getName().string());

        // Write the string blocks.
        const size_t typeStringsStart = data->getSize();
        sp<AaptFile> strFile = p->getTypeStringsData();
        ssize_t amt = data->writeData(strFile->getData(), strFile->getSize());
        #if PRINT_STRING_METRICS
        fprintf(stderr, "**** type strings: %d\n", amt);
        #endif
        strAmt += amt;
        if (amt < 0) {
            return amt;
        }
        const size_t keyStringsStart = data->getSize();
        strFile = p->getKeyStringsData();
        amt = data->writeData(strFile->getData(), strFile->getSize());
        #if PRINT_STRING_METRICS
        fprintf(stderr, "**** key strings: %d\n", amt);
        #endif
        strAmt += amt;
        if (amt < 0) {
            return amt;
        }

        if (isBase) {
            status_t err = flattenLibraryTable(data, libraryPackages);
            if (err != NO_ERROR) {
                fprintf(stderr, "ERROR: failed to write library table\n");
                return err;
            }
        }

        // Build the type chunks inside of this package.
        for (size_t ti=0; ti<N; ti++) {
            // Retrieve them in the same order as the type string block.
            size_t len;
            String16 typeName(p->getTypeStrings().stringAt(ti, &len));
            sp<Type> t = p->getTypes().valueFor(typeName);
            LOG_ALWAYS_FATAL_IF(t == NULL && typeName != String16("<empty>"),
                                "Type name %s not found",
                                String8(typeName).string());
            if (t == NULL) {
                continue;
            }
            const bool filterable = (typeName != mipmap16);
            const bool skipEntireType = (typeName == mipmap16 && !isBase);

            const size_t N = t != NULL ? t->getOrderedConfigs().size() : 0;

            // Until a non-NO_ENTRY value has been written for a resource,
            // that resource is invalid; validResources[i] represents
            // the item at t->getOrderedConfigs().itemAt(i).
            Vector<bool> validResources;
            validResources.insertAt(false, 0, N);
            
            // First write the typeSpec chunk, containing information about
            // each resource entry in this type.
            {
                const size_t typeSpecSize = sizeof(ResTable_typeSpec) + sizeof(uint32_t)*N;
                const size_t typeSpecStart = data->getSize();
                ResTable_typeSpec* tsHeader = (ResTable_typeSpec*)
                    (((uint8_t*)data->editData(typeSpecStart+typeSpecSize)) + typeSpecStart);
                if (tsHeader == NULL) {
                    fprintf(stderr, "ERROR: out of memory creating ResTable_typeSpec\n");
                    return NO_MEMORY;
                }
                memset(tsHeader, 0, sizeof(*tsHeader));
                tsHeader->header.type = htods(RES_TABLE_TYPE_SPEC_TYPE);
                tsHeader->header.headerSize = htods(sizeof(*tsHeader));
                tsHeader->header.size = htodl(typeSpecSize);
                tsHeader->id = ti+1;
                tsHeader->entryCount = htodl(N);
                
                uint32_t* typeSpecFlags = (uint32_t*)
                    (((uint8_t*)data->editData())
                        + typeSpecStart + sizeof(ResTable_typeSpec));
                memset(typeSpecFlags, 0, sizeof(uint32_t)*N);

                for (size_t ei=0; ei<N; ei++) {
                    sp<ConfigList> cl = t->getOrderedConfigs().itemAt(ei);
                    if (cl->getPublic()) {
                        typeSpecFlags[ei] |= htodl(ResTable_typeSpec::SPEC_PUBLIC);
                    }

                    if (skipEntireType) {
                        continue;
                    }

                    const size_t CN = cl->getEntries().size();
                    for (size_t ci=0; ci<CN; ci++) {
                        if (filterable && !filter->match(cl->getEntries().keyAt(ci))) {
                            continue;
                        }
                        for (size_t cj=ci+1; cj<CN; cj++) {
                            if (filterable && !filter->match(cl->getEntries().keyAt(cj))) {
                                continue;
                            }
                            typeSpecFlags[ei] |= htodl(
                                cl->getEntries().keyAt(ci).diff(cl->getEntries().keyAt(cj)));
                        }
                    }
                }
            }
            
            if (skipEntireType) {
                continue;
            }

            // We need to write one type chunk for each configuration for
            // which we have entries in this type.
            const size_t NC = t->getUniqueConfigs().size();
            
            const size_t typeSize = sizeof(ResTable_type) + sizeof(uint32_t)*N;
            
            for (size_t ci=0; ci<NC; ci++) {
                ConfigDescription config = t->getUniqueConfigs().itemAt(ci);

                NOISY(printf("Writing config %d config: imsi:%d/%d lang:%c%c cnt:%c%c "
                     "orien:%d ui:%d touch:%d density:%d key:%d inp:%d nav:%d sz:%dx%d "
                     "sw%ddp w%ddp h%ddp dir:%d\n",
                      ti+1,
                      config.mcc, config.mnc,
                      config.language[0] ? config.language[0] : '-',
                      config.language[1] ? config.language[1] : '-',
                      config.country[0] ? config.country[0] : '-',
                      config.country[1] ? config.country[1] : '-',
                      config.orientation,
                      config.uiMode,
                      config.touchscreen,
                      config.density,
                      config.keyboard,
                      config.inputFlags,
                      config.navigation,
                      config.screenWidth,
                      config.screenHeight,
                      config.smallestScreenWidthDp,
                      config.screenWidthDp,
                      config.screenHeightDp,
                      config.layoutDirection));
                      
                if (filterable && !filter->match(config)) {
                    continue;
                }
                
                const size_t typeStart = data->getSize();

                ResTable_type* tHeader = (ResTable_type*)
                    (((uint8_t*)data->editData(typeStart+typeSize)) + typeStart);
                if (tHeader == NULL) {
                    fprintf(stderr, "ERROR: out of memory creating ResTable_type\n");
                    return NO_MEMORY;
                }

                memset(tHeader, 0, sizeof(*tHeader));
                tHeader->header.type = htods(RES_TABLE_TYPE_TYPE);
                tHeader->header.headerSize = htods(sizeof(*tHeader));
                tHeader->id = ti+1;
                tHeader->entryCount = htodl(N);
                tHeader->entriesStart = htodl(typeSize);
                tHeader->config = config;
                NOISY(printf("Writing type %d config: imsi:%d/%d lang:%c%c cnt:%c%c "
                     "orien:%d ui:%d touch:%d density:%d key:%d inp:%d nav:%d sz:%dx%d "
                     "sw%ddp w%ddp h%ddp dir:%d\n",
                      ti+1,
                      tHeader->config.mcc, tHeader->config.mnc,
                      tHeader->config.language[0] ? tHeader->config.language[0] : '-',
                      tHeader->config.language[1] ? tHeader->config.language[1] : '-',
                      tHeader->config.country[0] ? tHeader->config.country[0] : '-',
                      tHeader->config.country[1] ? tHeader->config.country[1] : '-',
                      tHeader->config.orientation,
                      tHeader->config.uiMode,
                      tHeader->config.touchscreen,
                      tHeader->config.density,
                      tHeader->config.keyboard,
                      tHeader->config.inputFlags,
                      tHeader->config.navigation,
                      tHeader->config.screenWidth,
                      tHeader->config.screenHeight,
                      tHeader->config.smallestScreenWidthDp,
                      tHeader->config.screenWidthDp,
                      tHeader->config.screenHeightDp,
                      tHeader->config.layoutDirection));
                tHeader->config.swapHtoD();

                // Build the entries inside of this type.
                for (size_t ei=0; ei<N; ei++) {
                    sp<ConfigList> cl = t->getOrderedConfigs().itemAt(ei);
                    sp<Entry> e = cl->getEntries().valueFor(config);

                    // Set the offset for this entry in its type.
                    uint32_t* index = (uint32_t*)
                        (((uint8_t*)data->editData())
                            + typeStart + sizeof(ResTable_type));
                    if (e != NULL) {
                        index[ei] = htodl(data->getSize()-typeStart-typeSize);

                        // Create the entry.
                        ssize_t amt = e->flatten(bundle, data, cl->getPublic());
                        if (amt < 0) {
                            return amt;
                        }
                        validResources.editItemAt(ei) = true;
                    } else {
                        index[ei] = htodl(ResTable_type::NO_ENTRY);
                    }
                }

                // Fill in the rest of the type information.
                tHeader = (ResTable_type*)
                    (((uint8_t*)data->editData()) + typeStart);
                tHeader->header.size = htodl(data->getSize()-typeStart);
            }

            // If we're building splits, then each invocation of the flattening
            // step will have 'missing' entries. Don't warn/error for this case.
            if (bundle->getSplitConfigurations().isEmpty()) {
                bool missing_entry = false;
                const char* log_prefix = bundle->getErrorOnMissingConfigEntry() ?
                        "error" : "warning";
                for (size_t i = 0; i < N; ++i) {
                    if (!validResources[i]) {
                        sp<ConfigList> c = t->getOrderedConfigs().itemAt(i);
                        fprintf(stderr, "%s: no entries written for %s/%s (0x%08x)\n", log_prefix,
                                String8(typeName).string(), String8(c->getName()).string(),
                                Res_MAKEID(p->getAssignedId() - 1, ti, i));
                        missing_entry = true;
                    }
                }
                if (bundle->getErrorOnMissingConfigEntry() && missing_entry) {
                    fprintf(stderr, "Error: Missing entries, quit!\n");
                    return NOT_ENOUGH_DATA;
                }
            }
        }

        // Fill in the rest of the package information.
        header = (ResTable_package*)data->editData();
        header->header.size = htodl(data->getSize());
        header->typeStrings = htodl(typeStringsStart);
        header->lastPublicType = htodl(p->getTypeStrings().size());
        header->keyStrings = htodl(keyStringsStart);
        header->lastPublicKey = htodl(p->getKeyStrings().size());

        flatPackages.add(data);
    }

    // And now write out the final chunks.
    const size_t dataStart = dest->getSize();

    {
        // blah
        ResTable_header header;
        memset(&header, 0, sizeof(header));
        header.header.type = htods(RES_TABLE_TYPE);
        header.header.headerSize = htods(sizeof(header));
        header.packageCount = htodl(flatPackages.size());
        status_t err = dest->writeData(&header, sizeof(header));
        if (err != NO_ERROR) {
            fprintf(stderr, "ERROR: out of memory creating ResTable_header\n");
            return err;
        }
    }
    
    ssize_t strStart = dest->getSize();
    status_t err = valueStrings.writeStringBlock(dest);
    if (err != NO_ERROR) {
        return err;
    }

    ssize_t amt = (dest->getSize()-strStart);
    strAmt += amt;
    #if PRINT_STRING_METRICS
    fprintf(stderr, "**** value strings: %d\n", amt);
    fprintf(stderr, "**** total strings: %d\n", strAmt);
    #endif

    for (pi=0; pi<flatPackages.size(); pi++) {
        err = dest->writeData(flatPackages[pi]->getData(),
                              flatPackages[pi]->getSize());
        if (err != NO_ERROR) {
            fprintf(stderr, "ERROR: out of memory creating package chunk for ResTable_header\n");
            return err;
        }
    }

    ResTable_header* header = (ResTable_header*)
        (((uint8_t*)dest->getData()) + dataStart);
    header->header.size = htodl(dest->getSize() - dataStart);

    NOISY(aout << "Resource table:"
          << HexDump(dest->getData(), dest->getSize()) << endl);

    #if PRINT_STRING_METRICS
    fprintf(stderr, "**** total resource table size: %d / %d%% strings\n",
        dest->getSize(), (strAmt*100)/dest->getSize());
    #endif
    
    return NO_ERROR;
}

status_t ResourceTable::flattenLibraryTable(const sp<AaptFile>& dest, const Vector<sp<Package> >& libs) {
    // Write out the library table if necessary
    if (libs.size() > 0) {
        NOISY(fprintf(stderr, "Writing library reference table\n"));

        const size_t libStart = dest->getSize();
        const size_t count = libs.size();
        ResTable_lib_header* libHeader = (ResTable_lib_header*) dest->editDataInRange(
                libStart, sizeof(ResTable_lib_header));

        memset(libHeader, 0, sizeof(*libHeader));
        libHeader->header.type = htods(RES_TABLE_LIBRARY_TYPE);
        libHeader->header.headerSize = htods(sizeof(*libHeader));
        libHeader->header.size = htodl(sizeof(*libHeader) + (sizeof(ResTable_lib_entry) * count));
        libHeader->count = htodl(count);

        // Write the library entries
        for (size_t i = 0; i < count; i++) {
            const size_t entryStart = dest->getSize();
            sp<Package> libPackage = libs[i];
            NOISY(fprintf(stderr, "  Entry %s -> 0x%02x\n",
                        String8(libPackage->getName()).string(),
                        (uint8_t)libPackage->getAssignedId()));

            ResTable_lib_entry* entry = (ResTable_lib_entry*) dest->editDataInRange(
                    entryStart, sizeof(ResTable_lib_entry));
            memset(entry, 0, sizeof(*entry));
            entry->packageId = htodl(libPackage->getAssignedId());
            strcpy16_htod(entry->packageName, libPackage->getName().string());
        }
    }
    return NO_ERROR;
}

void ResourceTable::writePublicDefinitions(const String16& package, FILE* fp)
{
    fprintf(fp,
    "<!-- This file contains <public> resource definitions for all\n"
    "     resources that were generated from the source data. -->\n"
    "\n"
    "<resources>\n");

    writePublicDefinitions(package, fp, true);
    writePublicDefinitions(package, fp, false);

    fprintf(fp,
    "\n"
    "</resources>\n");
}

void ResourceTable::writePublicDefinitions(const String16& package, FILE* fp, bool pub)
{
    bool didHeader = false;

    sp<Package> pkg = mPackages.valueFor(package);
    if (pkg != NULL) {
        const size_t NT = pkg->getOrderedTypes().size();
        for (size_t i=0; i<NT; i++) {
            sp<Type> t = pkg->getOrderedTypes().itemAt(i);
            if (t == NULL) {
                continue;
            }

            bool didType = false;

            const size_t NC = t->getOrderedConfigs().size();
            for (size_t j=0; j<NC; j++) {
                sp<ConfigList> c = t->getOrderedConfigs().itemAt(j);
                if (c == NULL) {
                    continue;
                }

                if (c->getPublic() != pub) {
                    continue;
                }

                if (!didType) {
                    fprintf(fp, "\n");
                    didType = true;
                }
                if (!didHeader) {
                    if (pub) {
                        fprintf(fp,"  <!-- PUBLIC SECTION.  These resources have been declared public.\n");
                        fprintf(fp,"       Changes to these definitions will break binary compatibility. -->\n\n");
                    } else {
                        fprintf(fp,"  <!-- PRIVATE SECTION.  These resources have not been declared public.\n");
                        fprintf(fp,"       You can make them public my moving these lines into a file in res/values. -->\n\n");
                    }
                    didHeader = true;
                }
                if (!pub) {
                    const size_t NE = c->getEntries().size();
                    for (size_t k=0; k<NE; k++) {
                        const SourcePos& pos = c->getEntries().valueAt(k)->getPos();
                        if (pos.file != "") {
                            fprintf(fp,"  <!-- Declared at %s:%d -->\n",
                                    pos.file.string(), pos.line);
                        }
                    }
                }
                fprintf(fp, "  <public type=\"%s\" name=\"%s\" id=\"0x%08x\" />\n",
                        String8(t->getName()).string(),
                        String8(c->getName()).string(),
                        getResId(pkg, t, c->getEntryIndex()));
            }
        }
    }
}

ResourceTable::Item::Item(const SourcePos& _sourcePos,
                          bool _isId,
                          const String16& _value,
                          const Vector<StringPool::entry_style_span>* _style,
                          int32_t _format)
    : sourcePos(_sourcePos)
    , isId(_isId)
    , value(_value)
    , format(_format)
    , bagKeyId(0)
    , evaluating(false)
{
    if (_style) {
        style = *_style;
    }
}

ResourceTable::Entry::Entry(const Entry& entry)
    : RefBase()
    , mName(entry.mName)
    , mParent(entry.mParent)
    , mType(entry.mType)
    , mItem(entry.mItem)
    , mItemFormat(entry.mItemFormat)
    , mBag(entry.mBag)
    , mNameIndex(entry.mNameIndex)
    , mParentId(entry.mParentId)
    , mPos(entry.mPos) {}

ResourceTable::Entry& ResourceTable::Entry::operator=(const Entry& entry) {
    mName = entry.mName;
    mParent = entry.mParent;
    mType = entry.mType;
    mItem = entry.mItem;
    mItemFormat = entry.mItemFormat;
    mBag = entry.mBag;
    mNameIndex = entry.mNameIndex;
    mParentId = entry.mParentId;
    mPos = entry.mPos;
    return *this;
}

status_t ResourceTable::Entry::makeItABag(const SourcePos& sourcePos)
{
    if (mType == TYPE_BAG) {
        return NO_ERROR;
    }
    if (mType == TYPE_UNKNOWN) {
        mType = TYPE_BAG;
        return NO_ERROR;
    }
    sourcePos.error("Resource entry %s is already defined as a single item.\n"
                    "%s:%d: Originally defined here.\n",
                    String8(mName).string(),
                    mItem.sourcePos.file.string(), mItem.sourcePos.line);
    return UNKNOWN_ERROR;
}

status_t ResourceTable::Entry::setItem(const SourcePos& sourcePos,
                                       const String16& value,
                                       const Vector<StringPool::entry_style_span>* style,
                                       int32_t format,
                                       const bool overwrite)
{
    Item item(sourcePos, false, value, style);

    if (mType == TYPE_BAG) {
        if (mBag.size() == 0) {
            sourcePos.error("Resource entry %s is already defined as a bag.",
                    String8(mName).string());
        } else {
            const Item& item(mBag.valueAt(0));
            sourcePos.error("Resource entry %s is already defined as a bag.\n"
                            "%s:%d: Originally defined here.\n",
                            String8(mName).string(),
                            item.sourcePos.file.string(), item.sourcePos.line);
        }
        return UNKNOWN_ERROR;
    }
    if ( (mType != TYPE_UNKNOWN) && (overwrite == false) ) {
        sourcePos.error("Resource entry %s is already defined.\n"
                        "%s:%d: Originally defined here.\n",
                        String8(mName).string(),
                        mItem.sourcePos.file.string(), mItem.sourcePos.line);
        return UNKNOWN_ERROR;
    }

    mType = TYPE_ITEM;
    mItem = item;
    mItemFormat = format;
    return NO_ERROR;
}

status_t ResourceTable::Entry::addToBag(const SourcePos& sourcePos,
                                        const String16& key, const String16& value,
                                        const Vector<StringPool::entry_style_span>* style,
                                        bool replace, bool isId, int32_t format)
{
    status_t err = makeItABag(sourcePos);
    if (err != NO_ERROR) {
        return err;
    }

    Item item(sourcePos, isId, value, style, format);
    
    // XXX NOTE: there is an error if you try to have a bag with two keys,
    // one an attr and one an id, with the same name.  Not something we
    // currently ever have to worry about.
    ssize_t origKey = mBag.indexOfKey(key);
    if (origKey >= 0) {
        if (!replace) {
            const Item& item(mBag.valueAt(origKey));
            sourcePos.error("Resource entry %s already has bag item %s.\n"
                    "%s:%d: Originally defined here.\n",
                    String8(mName).string(), String8(key).string(),
                    item.sourcePos.file.string(), item.sourcePos.line);
            return UNKNOWN_ERROR;
        }
        //printf("Replacing %s with %s\n",
        //       String8(mBag.valueFor(key).value).string(), String8(value).string());
        mBag.replaceValueFor(key, item);
    }

    mBag.add(key, item);
    return NO_ERROR;
}

status_t ResourceTable::Entry::removeFromBag(const String16& key) {
    if (mType != Entry::TYPE_BAG) {
        return NO_ERROR;
    }

    if (mBag.removeItem(key) >= 0) {
        return NO_ERROR;
    }
    return UNKNOWN_ERROR;
}

status_t ResourceTable::Entry::emptyBag(const SourcePos& sourcePos)
{
    status_t err = makeItABag(sourcePos);
    if (err != NO_ERROR) {
        return err;
    }

    mBag.clear();
    return NO_ERROR;
}

status_t ResourceTable::Entry::generateAttributes(ResourceTable* table,
                                                  const String16& package)
{
    const String16 attr16("attr");
    const String16 id16("id");
    const size_t N = mBag.size();
    for (size_t i=0; i<N; i++) {
        const String16& key = mBag.keyAt(i);
        const Item& it = mBag.valueAt(i);
        if (it.isId) {
            if (!table->hasBagOrEntry(key, &id16, &package)) {
                String16 value("false");
                NOISY(fprintf(stderr, "Generating %s:id/%s\n",
                        String8(package).string(),
                        String8(key).string()));
                status_t err = table->addEntry(SourcePos(String8("<generated>"), 0), package,
                                               id16, key, value);
                if (err != NO_ERROR) {
                    return err;
                }
            }
        } else if (!table->hasBagOrEntry(key, &attr16, &package)) {

#if 1
//             fprintf(stderr, "ERROR: Bag attribute '%s' has not been defined.\n",
//                     String8(key).string());
//             const Item& item(mBag.valueAt(i));
//             fprintf(stderr, "Referenced from file %s line %d\n",
//                     item.sourcePos.file.string(), item.sourcePos.line);
//             return UNKNOWN_ERROR;
#else
            char numberStr[16];
            sprintf(numberStr, "%d", ResTable_map::TYPE_ANY);
            status_t err = table->addBag(SourcePos("<generated>", 0), package,
                                         attr16, key, String16(""),
                                         String16("^type"),
                                         String16(numberStr), NULL, NULL);
            if (err != NO_ERROR) {
                return err;
            }
#endif
        }
    }
    return NO_ERROR;
}

status_t ResourceTable::Entry::assignResourceIds(ResourceTable* table,
                                                 const String16& package)
{
    bool hasErrors = false;
    
    if (mType == TYPE_BAG) {
        const char* errorMsg;
        const String16 style16("style");
        const String16 attr16("attr");
        const String16 id16("id");
        mParentId = 0;
        if (mParent.size() > 0) {
            mParentId = table->getResId(mParent, &style16, NULL, &errorMsg);
            if (mParentId == 0) {
                mPos.error("Error retrieving parent for item: %s '%s'.\n",
                        errorMsg, String8(mParent).string());
                hasErrors = true;
            }
        }
        const size_t N = mBag.size();
        for (size_t i=0; i<N; i++) {
            const String16& key = mBag.keyAt(i);
            Item& it = mBag.editValueAt(i);
            it.bagKeyId = table->getResId(key,
                    it.isId ? &id16 : &attr16, NULL, &errorMsg);
            //printf("Bag key of %s: #%08x\n", String8(key).string(), it.bagKeyId);
            if (it.bagKeyId == 0) {
                it.sourcePos.error("Error: %s: %s '%s'.\n", errorMsg,
                        String8(it.isId ? id16 : attr16).string(),
                        String8(key).string());
                hasErrors = true;
            }
        }
    }
    return hasErrors ? UNKNOWN_ERROR : NO_ERROR;
}

status_t ResourceTable::Entry::prepareFlatten(StringPool* strings, ResourceTable* table,
        const String8* configTypeName, const ConfigDescription* config)
{
    if (mType == TYPE_ITEM) {
        Item& it = mItem;
        AccessorCookie ac(it.sourcePos, String8(mName), String8(it.value));
        if (!table->stringToValue(&it.parsedValue, strings,
                                  it.value, false, true, 0,
                                  &it.style, NULL, &ac, mItemFormat,
                                  configTypeName, config)) {
            return UNKNOWN_ERROR;
        }
    } else if (mType == TYPE_BAG) {
        const size_t N = mBag.size();
        for (size_t i=0; i<N; i++) {
            const String16& key = mBag.keyAt(i);
            Item& it = mBag.editValueAt(i);
            AccessorCookie ac(it.sourcePos, String8(key), String8(it.value));
            if (!table->stringToValue(&it.parsedValue, strings,
                                      it.value, false, true, it.bagKeyId,
                                      &it.style, NULL, &ac, it.format,
                                      configTypeName, config)) {
                return UNKNOWN_ERROR;
            }
        }
    } else {
        mPos.error("Error: entry %s is not a single item or a bag.\n",
                   String8(mName).string());
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t ResourceTable::Entry::remapStringValue(StringPool* strings)
{
    if (mType == TYPE_ITEM) {
        Item& it = mItem;
        if (it.parsedValue.dataType == Res_value::TYPE_STRING) {
            it.parsedValue.data = strings->mapOriginalPosToNewPos(it.parsedValue.data);
        }
    } else if (mType == TYPE_BAG) {
        const size_t N = mBag.size();
        for (size_t i=0; i<N; i++) {
            Item& it = mBag.editValueAt(i);
            if (it.parsedValue.dataType == Res_value::TYPE_STRING) {
                it.parsedValue.data = strings->mapOriginalPosToNewPos(it.parsedValue.data);
            }
        }
    } else {
        mPos.error("Error: entry %s is not a single item or a bag.\n",
                   String8(mName).string());
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

ssize_t ResourceTable::Entry::flatten(Bundle* bundle, const sp<AaptFile>& data, bool isPublic)
{
    size_t amt = 0;
    ResTable_entry header;
    memset(&header, 0, sizeof(header));
    header.size = htods(sizeof(header));
    const type ty = this != NULL ? mType : TYPE_ITEM;
    if (this != NULL) {
        if (ty == TYPE_BAG) {
            header.flags |= htods(header.FLAG_COMPLEX);
        }
        if (isPublic) {
            header.flags |= htods(header.FLAG_PUBLIC);
        }
        header.key.index = htodl(mNameIndex);
    }
    if (ty != TYPE_BAG) {
        status_t err = data->writeData(&header, sizeof(header));
        if (err != NO_ERROR) {
            fprintf(stderr, "ERROR: out of memory creating ResTable_entry\n");
            return err;
        }

        const Item& it = mItem;
        Res_value par;
        memset(&par, 0, sizeof(par));
        par.size = htods(it.parsedValue.size);
        par.dataType = it.parsedValue.dataType;
        par.res0 = it.parsedValue.res0;
        par.data = htodl(it.parsedValue.data);
        #if 0
        printf("Writing item (%s): type=%d, data=0x%x, res0=0x%x\n",
               String8(mName).string(), it.parsedValue.dataType,
               it.parsedValue.data, par.res0);
        #endif
        err = data->writeData(&par, it.parsedValue.size);
        if (err != NO_ERROR) {
            fprintf(stderr, "ERROR: out of memory creating Res_value\n");
            return err;
        }
        amt += it.parsedValue.size;
    } else {
        size_t N = mBag.size();
        size_t i;
        // Create correct ordering of items.
        KeyedVector<uint32_t, const Item*> items;
        for (i=0; i<N; i++) {
            const Item& it = mBag.valueAt(i);
            items.add(it.bagKeyId, &it);
        }
        N = items.size();
        
        ResTable_map_entry mapHeader;
        memcpy(&mapHeader, &header, sizeof(header));
        mapHeader.size = htods(sizeof(mapHeader));
        mapHeader.parent.ident = htodl(mParentId);
        mapHeader.count = htodl(N);
        status_t err = data->writeData(&mapHeader, sizeof(mapHeader));
        if (err != NO_ERROR) {
            fprintf(stderr, "ERROR: out of memory creating ResTable_entry\n");
            return err;
        }

        for (i=0; i<N; i++) {
            const Item& it = *items.valueAt(i);
            ResTable_map map;
            map.name.ident = htodl(it.bagKeyId);
            map.value.size = htods(it.parsedValue.size);
            map.value.dataType = it.parsedValue.dataType;
            map.value.res0 = it.parsedValue.res0;
            map.value.data = htodl(it.parsedValue.data);
            err = data->writeData(&map, sizeof(map));
            if (err != NO_ERROR) {
                fprintf(stderr, "ERROR: out of memory creating Res_value\n");
                return err;
            }
            amt += sizeof(map);
        }
    }
    return amt;
}

void ResourceTable::ConfigList::appendComment(const String16& comment,
                                              bool onlyIfEmpty)
{
    if (comment.size() <= 0) {
        return;
    }
    if (onlyIfEmpty && mComment.size() > 0) {
        return;
    }
    if (mComment.size() > 0) {
        mComment.append(String16("\n"));
    }
    mComment.append(comment);
}

void ResourceTable::ConfigList::appendTypeComment(const String16& comment)
{
    if (comment.size() <= 0) {
        return;
    }
    if (mTypeComment.size() > 0) {
        mTypeComment.append(String16("\n"));
    }
    mTypeComment.append(comment);
}

status_t ResourceTable::Type::addPublic(const SourcePos& sourcePos,
                                        const String16& name,
                                        const uint32_t ident)
{
    #if 0
    int32_t entryIdx = Res_GETENTRY(ident);
    if (entryIdx < 0) {
        sourcePos.error("Public resource %s/%s has an invalid 0 identifier (0x%08x).\n",
                String8(mName).string(), String8(name).string(), ident);
        return UNKNOWN_ERROR;
    }
    #endif

    int32_t typeIdx = Res_GETTYPE(ident);
    if (typeIdx >= 0) {
        typeIdx++;
        if (mPublicIndex > 0 && mPublicIndex != typeIdx) {
            sourcePos.error("Public resource %s/%s has conflicting type codes for its"
                    " public identifiers (0x%x vs 0x%x).\n",
                    String8(mName).string(), String8(name).string(),
                    mPublicIndex, typeIdx);
            return UNKNOWN_ERROR;
        }
        mPublicIndex = typeIdx;
    }

    if (mFirstPublicSourcePos == NULL) {
        mFirstPublicSourcePos = new SourcePos(sourcePos);
    }

    if (mPublic.indexOfKey(name) < 0) {
        mPublic.add(name, Public(sourcePos, String16(), ident));
    } else {
        Public& p = mPublic.editValueFor(name);
        if (p.ident != ident) {
            sourcePos.error("Public resource %s/%s has conflicting public identifiers"
                    " (0x%08x vs 0x%08x).\n"
                    "%s:%d: Originally defined here.\n",
                    String8(mName).string(), String8(name).string(), p.ident, ident,
                    p.sourcePos.file.string(), p.sourcePos.line);
            return UNKNOWN_ERROR;
        }
    }

    return NO_ERROR;
}

void ResourceTable::Type::canAddEntry(const String16& name)
{
    mCanAddEntries.add(name);
}

sp<ResourceTable::Entry> ResourceTable::Type::getEntry(const String16& entry,
                                                       const SourcePos& sourcePos,
                                                       const ResTable_config* config,
                                                       bool doSetIndex,
                                                       bool overlay,
                                                       bool autoAddOverlay)
{
    int pos = -1;
    sp<ConfigList> c = mConfigs.valueFor(entry);
    if (c == NULL) {
        if (overlay && !autoAddOverlay && mCanAddEntries.indexOf(entry) < 0) {
            sourcePos.error("Resource at %s appears in overlay but not"
                            " in the base package; use <add-resource> to add.\n",
                            String8(entry).string());
            return NULL;
        }
        c = new ConfigList(entry, sourcePos);
        mConfigs.add(entry, c);
        pos = (int)mOrderedConfigs.size();
        mOrderedConfigs.add(c);
        if (doSetIndex) {
            c->setEntryIndex(pos);
        }
    }
    
    ConfigDescription cdesc;
    if (config) cdesc = *config;
    
    sp<Entry> e = c->getEntries().valueFor(cdesc);
    if (e == NULL) {
        if (config != NULL) {
            NOISY(printf("New entry at %s:%d: imsi:%d/%d lang:%c%c cnt:%c%c "
                    "orien:%d touch:%d density:%d key:%d inp:%d nav:%d sz:%dx%d "
                    "sw%ddp w%ddp h%ddp dir:%d\n",
                      sourcePos.file.string(), sourcePos.line,
                      config->mcc, config->mnc,
                      config->language[0] ? config->language[0] : '-',
                      config->language[1] ? config->language[1] : '-',
                      config->country[0] ? config->country[0] : '-',
                      config->country[1] ? config->country[1] : '-',
                      config->orientation,
                      config->touchscreen,
                      config->density,
                      config->keyboard,
                      config->inputFlags,
                      config->navigation,
                      config->screenWidth,
                      config->screenHeight,
                      config->smallestScreenWidthDp,
                      config->screenWidthDp,
                      config->screenHeightDp,
                      config->layoutDirection));
        } else {
            NOISY(printf("New entry at %s:%d: NULL config\n",
                      sourcePos.file.string(), sourcePos.line));
        }
        e = new Entry(entry, sourcePos);
        c->addEntry(cdesc, e);
        /*
        if (doSetIndex) {
            if (pos < 0) {
                for (pos=0; pos<(int)mOrderedConfigs.size(); pos++) {
                    if (mOrderedConfigs[pos] == c) {
                        break;
                    }
                }
                if (pos >= (int)mOrderedConfigs.size()) {
                    sourcePos.error("Internal error: config not found in mOrderedConfigs when adding entry");
                    return NULL;
                }
            }
            e->setEntryIndex(pos);
        }
        */
    }
    
    mUniqueConfigs.add(cdesc);
    
    return e;
}

status_t ResourceTable::Type::applyPublicEntryOrder()
{
    size_t N = mOrderedConfigs.size();
    Vector<sp<ConfigList> > origOrder(mOrderedConfigs);
    bool hasError = false;

    size_t i;
    for (i=0; i<N; i++) {
        mOrderedConfigs.replaceAt(NULL, i);
    }

    const size_t NP = mPublic.size();
    //printf("Ordering %d configs from %d public defs\n", N, NP);
    size_t j;
    for (j=0; j<NP; j++) {
        const String16& name = mPublic.keyAt(j);
        const Public& p = mPublic.valueAt(j);
        int32_t idx = Res_GETENTRY(p.ident);
        //printf("Looking for entry \"%s\"/\"%s\" (0x%08x) in %d...\n",
        //       String8(mName).string(), String8(name).string(), p.ident, N);
        bool found = false;
        for (i=0; i<N; i++) {
            sp<ConfigList> e = origOrder.itemAt(i);
            //printf("#%d: \"%s\"\n", i, String8(e->getName()).string());
            if (e->getName() == name) {
                if (idx >= (int32_t)mOrderedConfigs.size()) {
                    p.sourcePos.error("Public entry identifier 0x%x entry index "
                            "is larger than available symbols (index %d, total symbols %d).\n",
                            p.ident, idx, mOrderedConfigs.size());
                    hasError = true;
                } else if (mOrderedConfigs.itemAt(idx) == NULL) {
                    e->setPublic(true);
                    e->setPublicSourcePos(p.sourcePos);
                    mOrderedConfigs.replaceAt(e, idx);
                    origOrder.removeAt(i);
                    N--;
                    found = true;
                    break;
                } else {
                    sp<ConfigList> oe = mOrderedConfigs.itemAt(idx);

                    p.sourcePos.error("Multiple entry names declared for public entry"
                            " identifier 0x%x in type %s (%s vs %s).\n"
                            "%s:%d: Originally defined here.",
                            idx+1, String8(mName).string(),
                            String8(oe->getName()).string(),
                            String8(name).string(),
                            oe->getPublicSourcePos().file.string(),
                            oe->getPublicSourcePos().line);
                    hasError = true;
                }
            }
        }

        if (!found) {
            p.sourcePos.error("Public symbol %s/%s declared here is not defined.",
                    String8(mName).string(), String8(name).string());
            hasError = true;
        }
    }

    //printf("Copying back in %d non-public configs, have %d\n", N, origOrder.size());
    
    if (N != origOrder.size()) {
        printf("Internal error: remaining private symbol count mismatch\n");
        N = origOrder.size();
    }
    
    j = 0;
    for (i=0; i<N; i++) {
        sp<ConfigList> e = origOrder.itemAt(i);
        // There will always be enough room for the remaining entries.
        while (mOrderedConfigs.itemAt(j) != NULL) {
            j++;
        }
        mOrderedConfigs.replaceAt(e, j);
        j++;
    }

    return hasError ? UNKNOWN_ERROR : NO_ERROR;
}

ResourceTable::Package::Package(const String16& name, size_t packageId)
    : mName(name), mPackageId(packageId),
      mTypeStringsMapping(0xffffffff),
      mKeyStringsMapping(0xffffffff)
{
}

sp<ResourceTable::Type> ResourceTable::Package::getType(const String16& type,
                                                        const SourcePos& sourcePos,
                                                        bool doSetIndex)
{
    sp<Type> t = mTypes.valueFor(type);
    if (t == NULL) {
        t = new Type(type, sourcePos);
        mTypes.add(type, t);
        mOrderedTypes.add(t);
        if (doSetIndex) {
            // For some reason the type's index is set to one plus the index
            // in the mOrderedTypes list, rather than just the index.
            t->setIndex(mOrderedTypes.size());
        }
    }
    return t;
}

status_t ResourceTable::Package::setTypeStrings(const sp<AaptFile>& data)
{
    status_t err = setStrings(data, &mTypeStrings, &mTypeStringsMapping);
    if (err != NO_ERROR) {
        fprintf(stderr, "ERROR: Type string data is corrupt!\n");
        return err;
    }

    // Retain a reference to the new data after we've successfully replaced
    // all uses of the old reference (in setStrings() ).
    mTypeStringsData = data;
    return NO_ERROR;
}

status_t ResourceTable::Package::setKeyStrings(const sp<AaptFile>& data)
{
    status_t err = setStrings(data, &mKeyStrings, &mKeyStringsMapping);
    if (err != NO_ERROR) {
        fprintf(stderr, "ERROR: Key string data is corrupt!\n");
        return err;
    }

    // Retain a reference to the new data after we've successfully replaced
    // all uses of the old reference (in setStrings() ).
    mKeyStringsData = data;
    return NO_ERROR;
}

status_t ResourceTable::Package::setStrings(const sp<AaptFile>& data,
                                            ResStringPool* strings,
                                            DefaultKeyedVector<String16, uint32_t>* mappings)
{
    if (data->getData() == NULL) {
        return UNKNOWN_ERROR;
    }

    NOISY(aout << "Setting restable string pool: "
          << HexDump(data->getData(), data->getSize()) << endl);

    status_t err = strings->setTo(data->getData(), data->getSize());
    if (err == NO_ERROR) {
        const size_t N = strings->size();
        for (size_t i=0; i<N; i++) {
            size_t len;
            mappings->add(String16(strings->stringAt(i, &len)), i);
        }
    }
    return err;
}

status_t ResourceTable::Package::applyPublicTypeOrder()
{
    size_t N = mOrderedTypes.size();
    Vector<sp<Type> > origOrder(mOrderedTypes);

    size_t i;
    for (i=0; i<N; i++) {
        mOrderedTypes.replaceAt(NULL, i);
    }

    for (i=0; i<N; i++) {
        sp<Type> t = origOrder.itemAt(i);
        int32_t idx = t->getPublicIndex();
        if (idx > 0) {
            idx--;
            while (idx >= (int32_t)mOrderedTypes.size()) {
                mOrderedTypes.add();
            }
            if (mOrderedTypes.itemAt(idx) != NULL) {
                sp<Type> ot = mOrderedTypes.itemAt(idx);
                t->getFirstPublicSourcePos().error("Multiple type names declared for public type"
                        " identifier 0x%x (%s vs %s).\n"
                        "%s:%d: Originally defined here.",
                        idx, String8(ot->getName()).string(),
                        String8(t->getName()).string(),
                        ot->getFirstPublicSourcePos().file.string(),
                        ot->getFirstPublicSourcePos().line);
                return UNKNOWN_ERROR;
            }
            mOrderedTypes.replaceAt(t, idx);
            origOrder.removeAt(i);
            i--;
            N--;
        }
    }

    size_t j=0;
    for (i=0; i<N; i++) {
        sp<Type> t = origOrder.itemAt(i);
        // There will always be enough room for the remaining types.
        while (mOrderedTypes.itemAt(j) != NULL) {
            j++;
        }
        mOrderedTypes.replaceAt(t, j);
    }

    return NO_ERROR;
}

sp<ResourceTable::Package> ResourceTable::getPackage(const String16& package)
{
    if (package != mAssetsPackage) {
        return NULL;
    }
    return mPackages.valueFor(package);
}

sp<ResourceTable::Type> ResourceTable::getType(const String16& package,
                                               const String16& type,
                                               const SourcePos& sourcePos,
                                               bool doSetIndex)
{
    sp<Package> p = getPackage(package);
    if (p == NULL) {
        return NULL;
    }
    return p->getType(type, sourcePos, doSetIndex);
}

sp<ResourceTable::Entry> ResourceTable::getEntry(const String16& package,
                                                 const String16& type,
                                                 const String16& name,
                                                 const SourcePos& sourcePos,
                                                 bool overlay,
                                                 const ResTable_config* config,
                                                 bool doSetIndex)
{
    sp<Type> t = getType(package, type, sourcePos, doSetIndex);
    if (t == NULL) {
        return NULL;
    }
    return t->getEntry(name, sourcePos, config, doSetIndex, overlay, mBundle->getAutoAddOverlay());
}

sp<ResourceTable::ConfigList> ResourceTable::getConfigList(const String16& package,
        const String16& type, const String16& name) const
{
    const size_t packageCount = mOrderedPackages.size();
    for (size_t pi = 0; pi < packageCount; pi++) {
        const sp<Package>& p = mOrderedPackages[pi];
        if (p == NULL || p->getName() != package) {
            continue;
        }

        const Vector<sp<Type> >& types = p->getOrderedTypes();
        const size_t typeCount = types.size();
        for (size_t ti = 0; ti < typeCount; ti++) {
            const sp<Type>& t = types[ti];
            if (t == NULL || t->getName() != type) {
                continue;
            }

            const Vector<sp<ConfigList> >& configs = t->getOrderedConfigs();
            const size_t configCount = configs.size();
            for (size_t ci = 0; ci < configCount; ci++) {
                const sp<ConfigList>& cl = configs[ci];
                if (cl == NULL || cl->getName() != name) {
                    continue;
                }

                return cl;
            }
        }
    }
    return NULL;
}

sp<const ResourceTable::Entry> ResourceTable::getEntry(uint32_t resID,
                                                       const ResTable_config* config) const
{
    size_t pid = Res_GETPACKAGE(resID)+1;
    const size_t N = mOrderedPackages.size();
    sp<Package> p;
    for (size_t i = 0; i < N; i++) {
        sp<Package> check = mOrderedPackages[i];
        if (check->getAssignedId() == pid) {
            p = check;
            break;
        }

    }
    if (p == NULL) {
        fprintf(stderr, "warning: Package not found for resource #%08x\n", resID);
        return NULL;
    }

    int tid = Res_GETTYPE(resID);
    if (tid < 0 || tid >= (int)p->getOrderedTypes().size()) {
        fprintf(stderr, "warning: Type not found for resource #%08x\n", resID);
        return NULL;
    }
    sp<Type> t = p->getOrderedTypes()[tid];

    int eid = Res_GETENTRY(resID);
    if (eid < 0 || eid >= (int)t->getOrderedConfigs().size()) {
        fprintf(stderr, "warning: Entry not found for resource #%08x\n", resID);
        return NULL;
    }

    sp<ConfigList> c = t->getOrderedConfigs()[eid];
    if (c == NULL) {
        fprintf(stderr, "warning: Entry not found for resource #%08x\n", resID);
        return NULL;
    }
    
    ConfigDescription cdesc;
    if (config) cdesc = *config;
    sp<Entry> e = c->getEntries().valueFor(cdesc);
    if (c == NULL) {
        fprintf(stderr, "warning: Entry configuration not found for resource #%08x\n", resID);
        return NULL;
    }
    
    return e;
}

const ResourceTable::Item* ResourceTable::getItem(uint32_t resID, uint32_t attrID) const
{
    sp<const Entry> e = getEntry(resID);
    if (e == NULL) {
        return NULL;
    }

    const size_t N = e->getBag().size();
    for (size_t i=0; i<N; i++) {
        const Item& it = e->getBag().valueAt(i);
        if (it.bagKeyId == 0) {
            fprintf(stderr, "warning: ID not yet assigned to '%s' in bag '%s'\n",
                    String8(e->getName()).string(),
                    String8(e->getBag().keyAt(i)).string());
        }
        if (it.bagKeyId == attrID) {
            return &it;
        }
    }

    return NULL;
}

bool ResourceTable::getItemValue(
    uint32_t resID, uint32_t attrID, Res_value* outValue)
{
    const Item* item = getItem(resID, attrID);

    bool res = false;
    if (item != NULL) {
        if (item->evaluating) {
            sp<const Entry> e = getEntry(resID);
            const size_t N = e->getBag().size();
            size_t i;
            for (i=0; i<N; i++) {
                if (&e->getBag().valueAt(i) == item) {
                    break;
                }
            }
            fprintf(stderr, "warning: Circular reference detected in key '%s' of bag '%s'\n",
                    String8(e->getName()).string(),
                    String8(e->getBag().keyAt(i)).string());
            return false;
        }
        item->evaluating = true;
        res = stringToValue(outValue, NULL, item->value, false, false, item->bagKeyId);
        NOISY(
            if (res) {
                printf("getItemValue of #%08x[#%08x] (%s): type=#%08x, data=#%08x\n",
                       resID, attrID, String8(getEntry(resID)->getName()).string(),
                       outValue->dataType, outValue->data);
            } else {
                printf("getItemValue of #%08x[#%08x]: failed\n",
                       resID, attrID);
            }
        );
        item->evaluating = false;
    }
    return res;
}

/**
 * Returns true if the given attribute ID comes from
 * a platform version from or after L.
 */
bool ResourceTable::isAttributeFromL(uint32_t attrId) {
    const uint32_t baseAttrId = 0x010103f7;
    if ((attrId & 0xffff0000) != (baseAttrId & 0xffff0000)) {
        return false;
    }

    uint32_t specFlags;
    if (!mAssets->getIncludedResources().getResourceFlags(attrId, &specFlags)) {
        return false;
    }

    return (specFlags & ResTable_typeSpec::SPEC_PUBLIC) != 0 &&
        (attrId & 0x0000ffff) >= (baseAttrId & 0x0000ffff);
}

static bool isMinSdkVersionLOrAbove(const Bundle* bundle) {
    if (bundle->getMinSdkVersion() != NULL && strlen(bundle->getMinSdkVersion()) > 0) {
        const char firstChar = bundle->getMinSdkVersion()[0];
        if (firstChar >= 'L' && firstChar <= 'Z') {
            // L is the code-name for the v21 release.
            return true;
        }

        const int minSdk = atoi(bundle->getMinSdkVersion());
        if (minSdk >= SDK_L) {
            return true;
        }
    }
    return false;
}

/**
 * Modifies the entries in the resource table to account for compatibility
 * issues with older versions of Android.
 *
 * This primarily handles the issue of private/public attribute clashes
 * in framework resources.
 *
 * AAPT has traditionally assigned resource IDs to public attributes,
 * and then followed those public definitions with private attributes.
 *
 * --- PUBLIC ---
 * | 0x01010234 | attr/color
 * | 0x01010235 | attr/background
 *
 * --- PRIVATE ---
 * | 0x01010236 | attr/secret
 * | 0x01010237 | attr/shhh
 *
 * Each release, when attributes are added, they take the place of the private
 * attributes and the private attributes are shifted down again.
 *
 * --- PUBLIC ---
 * | 0x01010234 | attr/color
 * | 0x01010235 | attr/background
 * | 0x01010236 | attr/shinyNewAttr
 * | 0x01010237 | attr/highlyValuedFeature
 *
 * --- PRIVATE ---
 * | 0x01010238 | attr/secret
 * | 0x01010239 | attr/shhh
 *
 * Platform code may look for private attributes set in a theme. If an app
 * compiled against a newer version of the platform uses a new public
 * attribute that happens to have the same ID as the private attribute
 * the older platform is expecting, then the behavior is undefined.
 *
 * We get around this by detecting any newly defined attributes (in L),
 * copy the resource into a -v21 qualified resource, and delete the
 * attribute from the original resource. This ensures that older platforms
 * don't see the new attribute, but when running on L+ platforms, the
 * attribute will be respected.
 */
status_t ResourceTable::modifyForCompat(const Bundle* bundle) {
    if (isMinSdkVersionLOrAbove(bundle)) {
        // If this app will only ever run on L+ devices,
        // we don't need to do any compatibility work.
        return NO_ERROR;
    }

    const String16 attr16("attr");

    const size_t packageCount = mOrderedPackages.size();
    for (size_t pi = 0; pi < packageCount; pi++) {
        sp<Package> p = mOrderedPackages.itemAt(pi);
        if (p == NULL || p->getTypes().size() == 0) {
            // Empty, skip!
            continue;
        }

        const size_t typeCount = p->getOrderedTypes().size();
        for (size_t ti = 0; ti < typeCount; ti++) {
            sp<Type> t = p->getOrderedTypes().itemAt(ti);
            if (t == NULL) {
                continue;
            }

            const size_t configCount = t->getOrderedConfigs().size();
            for (size_t ci = 0; ci < configCount; ci++) {
                sp<ConfigList> c = t->getOrderedConfigs().itemAt(ci);
                if (c == NULL) {
                    continue;
                }

                Vector<key_value_pair_t<ConfigDescription, sp<Entry> > > entriesToAdd;
                const DefaultKeyedVector<ConfigDescription, sp<Entry> >& entries =
                        c->getEntries();
                const size_t entryCount = entries.size();
                for (size_t ei = 0; ei < entryCount; ei++) {
                    sp<Entry> e = entries.valueAt(ei);
                    if (e == NULL || e->getType() != Entry::TYPE_BAG) {
                        continue;
                    }

                    const ConfigDescription& config = entries.keyAt(ei);
                    if (config.sdkVersion >= SDK_L) {
                        // We don't need to do anything if the resource is
                        // already qualified for version 21 or higher.
                        continue;
                    }

                    Vector<String16> attributesToRemove;
                    const KeyedVector<String16, Item>& bag = e->getBag();
                    const size_t bagCount = bag.size();
                    for (size_t bi = 0; bi < bagCount; bi++) {
                        const Item& item = bag.valueAt(bi);
                        const uint32_t attrId = getResId(bag.keyAt(bi), &attr16);
                        if (isAttributeFromL(attrId)) {
                            attributesToRemove.add(bag.keyAt(bi));
                        }
                    }

                    if (attributesToRemove.isEmpty()) {
                        continue;
                    }

                    // Duplicate the entry under the same configuration
                    // but with sdkVersion == SDK_L.
                    ConfigDescription newConfig(config);
                    newConfig.sdkVersion = SDK_L;
                    entriesToAdd.add(key_value_pair_t<ConfigDescription, sp<Entry> >(
                            newConfig, new Entry(*e)));

                    // Remove the attribute from the original.
                    for (size_t i = 0; i < attributesToRemove.size(); i++) {
                        e->removeFromBag(attributesToRemove[i]);
                    }
                }

                const size_t entriesToAddCount = entriesToAdd.size();
                for (size_t i = 0; i < entriesToAddCount; i++) {
                    if (entries.indexOfKey(entriesToAdd[i].key) >= 0) {
                        // An entry already exists for this config.
                        // That means that any attributes that were
                        // defined in L in the original bag will be overriden
                        // anyways on L devices, so we do nothing.
                        continue;
                    }

                    if (bundle->getVerbose()) {
                        entriesToAdd[i].value->getPos()
                                .printf("using v%d attributes; synthesizing resource %s:%s/%s for configuration %s.",
                                        SDK_L,
                                        String8(p->getName()).string(),
                                        String8(t->getName()).string(),
                                        String8(entriesToAdd[i].value->getName()).string(),
                                        entriesToAdd[i].key.toString().string());
                    }

                    sp<Entry> newEntry = t->getEntry(c->getName(),
                            entriesToAdd[i].value->getPos(),
                            &entriesToAdd[i].key);

                    *newEntry = *entriesToAdd[i].value;
                }
            }
        }
    }
    return NO_ERROR;
}

status_t ResourceTable::modifyForCompat(const Bundle* bundle,
                                        const String16& resourceName,
                                        const sp<AaptFile>& target,
                                        const sp<XMLNode>& root) {
    if (isMinSdkVersionLOrAbove(bundle)) {
        return NO_ERROR;
    }

    if (target->getResourceType() == "" || target->getGroupEntry().toParams().sdkVersion >= SDK_L) {
        // Skip resources that have no type (AndroidManifest.xml) or are already version qualified with v21
        // or higher.
        return NO_ERROR;
    }

    Vector<key_value_pair_t<sp<XMLNode>, size_t> > attrsToRemove;

    Vector<sp<XMLNode> > nodesToVisit;
    nodesToVisit.push(root);
    while (!nodesToVisit.isEmpty()) {
        sp<XMLNode> node = nodesToVisit.top();
        nodesToVisit.pop();

        const Vector<XMLNode::attribute_entry>& attrs = node->getAttributes();
        const size_t attrCount = attrs.size();
        for (size_t i = 0; i < attrCount; i++) {
            const XMLNode::attribute_entry& attr = attrs[i];
            if (isAttributeFromL(attr.nameResId)) {
                attrsToRemove.add(key_value_pair_t<sp<XMLNode>, size_t>(node, i));
            }
        }

        // Schedule a visit to the children.
        const Vector<sp<XMLNode> >& children = node->getChildren();
        const size_t childCount = children.size();
        for (size_t i = 0; i < childCount; i++) {
            nodesToVisit.push(children[i]);
        }
    }

    if (attrsToRemove.isEmpty()) {
        return NO_ERROR;
    }

    ConfigDescription newConfig(target->getGroupEntry().toParams());
    newConfig.sdkVersion = SDK_L;

    // Look to see if we already have an overriding v21 configuration.
    sp<ConfigList> cl = getConfigList(String16(mAssets->getPackage()),
            String16(target->getResourceType()), resourceName);
    //if (cl == NULL) {
    //    fprintf(stderr, "fuuuuck\n");
    //}
    if (cl->getEntries().indexOfKey(newConfig) < 0) {
        // We don't have an overriding entry for v21, so we must duplicate this one.
        sp<XMLNode> newRoot = root->clone();
        sp<AaptFile> newFile = new AaptFile(target->getSourceFile(),
                AaptGroupEntry(newConfig), target->getResourceType());
        String8 resPath = String8::format("res/%s/%s",
                newFile->getGroupEntry().toDirName(target->getResourceType()).string(),
                target->getSourceFile().getPathLeaf().string());
        resPath.convertToResPath();

        // Add a resource table entry.
        if (bundle->getVerbose()) {
            SourcePos(target->getSourceFile(), -1).printf(
                    "using v%d attributes; synthesizing resource %s:%s/%s for configuration %s.",
                    SDK_L,
                    mAssets->getPackage().string(),
                    newFile->getResourceType().string(),
                    String8(resourceName).string(),
                    newConfig.toString().string());
        }

        addEntry(SourcePos(),
                String16(mAssets->getPackage()),
                String16(target->getResourceType()),
                resourceName,
                String16(resPath),
                NULL,
                &newConfig);

        // Schedule this to be compiled.
        CompileResourceWorkItem item;
        item.resourceName = resourceName;
        item.resPath = resPath;
        item.file = newFile;
        mWorkQueue.push(item);
    }

    const size_t removeCount = attrsToRemove.size();
    for (size_t i = 0; i < removeCount; i++) {
        sp<XMLNode> node = attrsToRemove[i].key;
        size_t attrIndex = attrsToRemove[i].value;
        const XMLNode::attribute_entry& ae = node->getAttributes()[attrIndex];
        if (bundle->getVerbose()) {
            SourcePos(node->getFilename(), node->getStartLineNumber()).printf(
                    "removing attribute %s%s%s from <%s>",
                    String8(ae.ns).string(),
                    (ae.ns.size() == 0 ? "" : ":"),
                    String8(ae.name).string(),
                    String8(node->getElementName()).string());
        }
        node->removeAttribute(attrIndex);
    }

    return NO_ERROR;
}
