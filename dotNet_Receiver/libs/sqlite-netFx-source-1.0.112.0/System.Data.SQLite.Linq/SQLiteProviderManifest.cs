/********************************************************
 * ADO.NET 2.0 Data Provider for SQLite Version 3.X
 * Written by Robert Simpson (robert@blackcastlesoft.com)
 * 
 * Released to the public domain, use at your own risk!
 ********************************************************/

#if USE_ENTITY_FRAMEWORK_6
namespace System.Data.SQLite.EF6
#else
namespace System.Data.SQLite.Linq
#endif
{
  using System;
  using System.Collections.Generic;
  using System.IO;
  using System.Reflection;

#if !PLATFORM_COMPACTFRAMEWORK
  using System.Text;
#endif

  using System.Xml;

#if USE_ENTITY_FRAMEWORK_6
  using System.Data.Entity.Core;
  using System.Data.Entity.Core.Common;
  using System.Data.Entity.Core.Metadata.Edm;
#else
  using System.Data.Common;
  using System.Data.Metadata.Edm;
#endif

  /// <summary>
  /// The Provider Manifest for SQL Server
  /// </summary>
  internal sealed class SQLiteProviderManifest : DbXmlEnabledProviderManifest
  {
    internal SQLiteDateFormats _dateTimeFormat;
    internal DateTimeKind _dateTimeKind;
    internal string _dateTimeFormatString;
    internal bool _binaryGuid;

    /// <summary>
    /// Constructs the provider manifest.
    /// </summary>
    /// <remarks>
    /// Previously, the manifest token was interpreted as a <see cref="SQLiteDateFormats" />,
    /// because the <see cref="DateTime" /> functions are vastly different depending on the
    /// connection was opened.  However, the manifest token may specify a connection string
    /// instead.  WARNING: Only the "DateTimeFormat", "DateTimeKind", "DateTimeFormatString",
    /// and "BinaryGUID" connection parameters are extracted from it.  All other connection
    /// parameters, if any are present, are silently ignored.
    /// </remarks>
    /// <param name="manifestToken">
    /// A token used to infer the capabilities of the store.
    /// </param>
    public SQLiteProviderManifest(string manifestToken)
      : base(GetProviderManifest())
    {
        SetFromOptions(ParseProviderManifestToken(GetProviderManifestToken(manifestToken)));
    }

    private static XmlReader GetProviderManifest()
    {
      return GetXmlResource("System.Data.SQLite.SQLiteProviderServices.ProviderManifest.xml");
    }

    /// <summary>
    /// Determines and returns the effective provider manifest token to use,
    /// based on the specified provider manifest token and the environment,
    /// if applicable.
    /// </summary>
    /// <param name="manifestToken">
    /// The original provider manifest token passed to the constructor for this
    /// class.
    /// </param>
    /// <returns>
    /// The effective provider manifest token.
    /// </returns>
    private static string GetProviderManifestToken(
        string manifestToken
        )
    {
#if !PLATFORM_COMPACTFRAMEWORK
        string value = UnsafeNativeMethods.GetSettingValue(
            "AppendManifestToken_SQLiteProviderManifest", null);

        if (String.IsNullOrEmpty(value))
            return manifestToken;

        int capacity = value.Length;

        if (manifestToken != null)
            capacity += manifestToken.Length;

        StringBuilder builder = new StringBuilder(capacity);

        builder.Append(manifestToken);
        builder.Append(value);

        return builder.ToString();
#else
        //
        // NOTE: The .NET Compact Framework lacks environment variable support.
        //       Therefore, just return the original provider manifest token
        //       verbatim in this case.
        //
        return manifestToken;
#endif
    }

    /// <summary>
    /// Attempts to parse a provider manifest token.  It must contain either a
    /// legacy string that specifies the <see cref="SQLiteDateFormats" /> value
    /// -OR- string that uses the standard connection string syntax; otherwise,
    /// the results are undefined.
    /// </summary>
    /// <param name="manifestToken">
    /// The manifest token to parse.
    /// </param>
    /// <returns>
    /// The dictionary containing the connection string parameters.
    /// </returns>
    private static SortedList<string, string> ParseProviderManifestToken(
        string manifestToken
        )
    {
        return SQLiteConnection.ParseConnectionString(manifestToken, false, true);
    }

    /// <summary>
    /// Attempts to set the provider manifest options from the specified
    /// connection string parameters.  An exception may be thrown if one
    /// or more of the connection string parameter values do not conform
    /// to the expected type.
    /// </summary>
    /// <param name="opts">
    /// The dictionary containing the connection string parameters.
    /// </param>
    internal void SetFromOptions(
        SortedList<string, string> opts
        )
    {
        _dateTimeFormat = SQLiteConnection.DefaultDateTimeFormat;
        _dateTimeKind = SQLiteConnection.DefaultDateTimeKind;
        _dateTimeFormatString = SQLiteConnection.DefaultDateTimeFormatString;
        _binaryGuid = /* SQLiteConnection.DefaultBinaryGUID; */ false; /* COMPAT: Legacy. */

        if (opts == null)
            return;

#if !PLATFORM_COMPACTFRAMEWORK
        string[] names = Enum.GetNames(typeof(SQLiteDateFormats));
#else
        string[] names = {
            SQLiteDateFormats.Ticks.ToString(),
            SQLiteDateFormats.ISO8601.ToString(),
            SQLiteDateFormats.JulianDay.ToString(),
            SQLiteDateFormats.UnixEpoch.ToString(),
            SQLiteDateFormats.InvariantCulture.ToString(),
            SQLiteDateFormats.CurrentCulture.ToString(),
            "Default"
        };
#endif

        foreach (string name in names)
        {
            if (String.IsNullOrEmpty(name))
                continue;

            object value = SQLiteConnection.FindKey(opts, name, null);

            if (value == null)
                continue;

            _dateTimeFormat = (SQLiteDateFormats)Enum.Parse(
                typeof(SQLiteDateFormats), name, true);
        }

        object enumValue;

        enumValue = SQLiteConnection.TryParseEnum(
            typeof(SQLiteDateFormats), SQLiteConnection.FindKey(
            opts, "DateTimeFormat", null), true);

        if (enumValue is SQLiteDateFormats)
            _dateTimeFormat = (SQLiteDateFormats)enumValue;

        enumValue = SQLiteConnection.TryParseEnum(
            typeof(DateTimeKind), SQLiteConnection.FindKey(
            opts, "DateTimeKind", null), true);

        if (enumValue is DateTimeKind)
            _dateTimeKind = (DateTimeKind)enumValue;

        string stringValue = SQLiteConnection.FindKey(
            opts, "DateTimeFormatString", null);

        if (stringValue != null)
            _dateTimeFormatString = stringValue;

        stringValue = SQLiteConnection.FindKey(
            opts, "BinaryGUID", null);

        if (stringValue != null)
            _binaryGuid = SQLiteConvert.ToBoolean(stringValue);
    }

    /// <summary>
    /// Returns manifest information for the provider
    /// </summary>
    /// <param name="informationType">The name of the information to be retrieved.</param>
    /// <returns>An XmlReader at the begining of the information requested.</returns>
    protected override XmlReader GetDbInformation(string informationType)
    {
      if (informationType == DbProviderManifest.StoreSchemaDefinition)
        return GetStoreSchemaDescription();
      else if (informationType == DbProviderManifest.StoreSchemaMapping)
        return GetStoreSchemaMapping();
      else if (informationType == DbProviderManifest.ConceptualSchemaDefinition)
        return null;

      throw new ProviderIncompatibleException(String.Format("SQLite does not support this information type '{0}'.", informationType));
    }

    /// <summary>
    /// This method takes a type and a set of facets and returns the best mapped equivalent type 
    /// in EDM.
    /// </summary>
    /// <param name="storeType">A TypeUsage encapsulating a store type and a set of facets</param>
    /// <returns>A TypeUsage encapsulating an EDM type and a set of facets</returns>
    public override TypeUsage GetEdmType(TypeUsage storeType)
    {
      if (storeType == null)
      {
        throw new ArgumentNullException("storeType");
      }

      string storeTypeName = storeType.EdmType.Name.ToLowerInvariant();
      //if (!base.StoreTypeNameToEdmPrimitiveType.ContainsKey(storeTypeName))
      //{
      //  switch (storeTypeName)
      //  {
      //    case "integer":
      //      return TypeUsage.CreateDefaultTypeUsage(PrimitiveType.GetEdmPrimitiveType(PrimitiveTypeKind.Int64));
      //    default:
      //      throw new ArgumentException(String.Format("SQLite does not support the type '{0}'.", storeTypeName));
      //  }
      //}

      PrimitiveType edmPrimitiveType;
      
      if (base.StoreTypeNameToEdmPrimitiveType.TryGetValue(storeTypeName, out edmPrimitiveType) == false)
        throw new ArgumentException(String.Format("SQLite does not support the type '{0}'.", storeTypeName));

      int maxLength = 0;
      bool isUnicode = true;
      bool isFixedLen = false;
      bool isUnbounded = true;

      PrimitiveTypeKind newPrimitiveTypeKind;

      switch (storeTypeName)
      {
        case "tinyint":
        case "smallint":
        case "integer":
        case "bit":
        case "uniqueidentifier":
        case "int":
        case "float":
        case "real":
          return TypeUsage.CreateDefaultTypeUsage(edmPrimitiveType);

        case "varchar":
          newPrimitiveTypeKind = PrimitiveTypeKind.String;
          isUnbounded = !TypeHelpers.TryGetMaxLength(storeType, out maxLength);
          isUnicode = false;
          isFixedLen = false;
          break;
        case "char":
          newPrimitiveTypeKind = PrimitiveTypeKind.String;
          isUnbounded = !TypeHelpers.TryGetMaxLength(storeType, out maxLength);
          isUnicode = false;
          isFixedLen = true;
          break;
        case "nvarchar":
          newPrimitiveTypeKind = PrimitiveTypeKind.String;
          isUnbounded = !TypeHelpers.TryGetMaxLength(storeType, out maxLength);
          isUnicode = true;
          isFixedLen = false;
          break;
        case "nchar":
          newPrimitiveTypeKind = PrimitiveTypeKind.String;
          isUnbounded = !TypeHelpers.TryGetMaxLength(storeType, out maxLength);
          isUnicode = true;
          isFixedLen = true;
          break;
        case "blob":
          newPrimitiveTypeKind = PrimitiveTypeKind.Binary;
          isUnbounded = !TypeHelpers.TryGetMaxLength(storeType, out maxLength);
          isFixedLen = false;
          break;
        case "decimal":
          {
            byte precision;
            byte scale;
            if (TypeHelpers.TryGetPrecision(storeType, out precision) && TypeHelpers.TryGetScale(storeType, out scale))
            {
              return TypeUsage.CreateDecimalTypeUsage(edmPrimitiveType, precision, scale);
            }
            else
            {
              return TypeUsage.CreateDecimalTypeUsage(edmPrimitiveType);
            }
          }
        case "datetime":
          return TypeUsage.CreateDateTimeTypeUsage(edmPrimitiveType, null);
        default:
          throw new NotSupportedException(String.Format("SQLite does not support the type '{0}'.", storeTypeName));
      }

      switch (newPrimitiveTypeKind)
      {
        case PrimitiveTypeKind.String:
          if (!isUnbounded)
          {
            return TypeUsage.CreateStringTypeUsage(edmPrimitiveType, isUnicode, isFixedLen, maxLength);
          }
          else
          {
            return TypeUsage.CreateStringTypeUsage(edmPrimitiveType, isUnicode, isFixedLen);
          }
        case PrimitiveTypeKind.Binary:
          if (!isUnbounded)
          {
            return TypeUsage.CreateBinaryTypeUsage(edmPrimitiveType, isFixedLen, maxLength);
          }
          else
          {
            return TypeUsage.CreateBinaryTypeUsage(edmPrimitiveType, isFixedLen);
          }
        default:
          throw new NotSupportedException(String.Format("SQLite does not support the type '{0}'.", storeTypeName));
      }
    }

    /// <summary>
    /// This method takes a type and a set of facets and returns the best mapped equivalent type 
    /// </summary>
    /// <param name="edmType">A TypeUsage encapsulating an EDM type and a set of facets</param>
    /// <returns>A TypeUsage encapsulating a store type and a set of facets</returns>
    public override TypeUsage GetStoreType(TypeUsage edmType)
    {
      if (edmType == null)
        throw new ArgumentNullException("edmType");

      PrimitiveType primitiveType = edmType.EdmType as PrimitiveType;
      if (primitiveType == null)
        throw new ArgumentException(String.Format("SQLite does not support the type '{0}'.", edmType));

      ReadOnlyMetadataCollection<Facet> facets = edmType.Facets;

      switch (primitiveType.PrimitiveTypeKind)
      {
        case PrimitiveTypeKind.Boolean:
          return TypeUsage.CreateDefaultTypeUsage(StoreTypeNameToStorePrimitiveType["bit"]);
        case PrimitiveTypeKind.Byte:
          return TypeUsage.CreateDefaultTypeUsage(StoreTypeNameToStorePrimitiveType["tinyint"]);
        case PrimitiveTypeKind.Int16:
          return TypeUsage.CreateDefaultTypeUsage(StoreTypeNameToStorePrimitiveType["smallint"]);
        case PrimitiveTypeKind.Int32:
          return TypeUsage.CreateDefaultTypeUsage(StoreTypeNameToStorePrimitiveType["int"]);
        case PrimitiveTypeKind.Int64:
          return TypeUsage.CreateDefaultTypeUsage(StoreTypeNameToStorePrimitiveType["integer"]);
        case PrimitiveTypeKind.Guid:
          return TypeUsage.CreateDefaultTypeUsage(StoreTypeNameToStorePrimitiveType["uniqueidentifier"]);
        case PrimitiveTypeKind.Double:
          return TypeUsage.CreateDefaultTypeUsage(StoreTypeNameToStorePrimitiveType["float"]);
        case PrimitiveTypeKind.Single:
          return TypeUsage.CreateDefaultTypeUsage(StoreTypeNameToStorePrimitiveType["real"]);
        case PrimitiveTypeKind.Decimal: // decimal, numeric, smallmoney, money
          {
            byte precision;
            if (!TypeHelpers.TryGetPrecision(edmType, out precision))
            {
              precision = 18;
            }

            byte scale;
            if (!TypeHelpers.TryGetScale(edmType, out scale))
            {
              scale = 0;
            }

            return TypeUsage.CreateDecimalTypeUsage(StoreTypeNameToStorePrimitiveType["decimal"], precision, scale);
          }
        case PrimitiveTypeKind.Binary: // binary, varbinary, varbinary(max), image, timestamp, rowversion
          {
            bool isFixedLength = null != facets["FixedLength"].Value && (bool)facets["FixedLength"].Value;
            Facet f = facets["MaxLength"];

            bool isMaxLength = f.IsUnbounded || null == f.Value || (int)f.Value > Int32.MaxValue;
            int maxLength = !isMaxLength ? (int)f.Value : Int32.MinValue;

            TypeUsage tu;
            if (isFixedLength)
              tu = TypeUsage.CreateBinaryTypeUsage(StoreTypeNameToStorePrimitiveType["blob"], true, maxLength);
            else
            {
              if (isMaxLength)
                tu = TypeUsage.CreateBinaryTypeUsage(StoreTypeNameToStorePrimitiveType["blob"], false);
              else
                tu = TypeUsage.CreateBinaryTypeUsage(StoreTypeNameToStorePrimitiveType["blob"], false, maxLength);
            }
            return tu;
          }
        case PrimitiveTypeKind.String: // char, nchar, varchar, nvarchar, varchar(max), nvarchar(max), ntext, text
          {
            bool isUnicode = null == facets["Unicode"].Value || (bool)facets["Unicode"].Value;
            bool isFixedLength = null != facets["FixedLength"].Value && (bool)facets["FixedLength"].Value;
            Facet f = facets["MaxLength"];
            // maxlen is true if facet value is unbounded, the value is bigger than the limited string sizes *or* the facet
            // value is null. this is needed since functions still have maxlength facet value as null
            bool isMaxLength = f.IsUnbounded || null == f.Value || (int)f.Value > (isUnicode ? Int32.MaxValue : Int32.MaxValue);
            int maxLength = !isMaxLength ? (int)f.Value : Int32.MinValue;

            TypeUsage tu;

            if (isUnicode)
            {
              if (isFixedLength)
                tu = TypeUsage.CreateStringTypeUsage(StoreTypeNameToStorePrimitiveType["nchar"], true, true, maxLength);
              else
              {
                if (isMaxLength)
                  tu = TypeUsage.CreateStringTypeUsage(StoreTypeNameToStorePrimitiveType["nvarchar"], true, false);
                else
                  tu = TypeUsage.CreateStringTypeUsage(StoreTypeNameToStorePrimitiveType["nvarchar"], true, false, maxLength);
              }
            }
            else
            {
              if (isFixedLength)
                tu = TypeUsage.CreateStringTypeUsage(StoreTypeNameToStorePrimitiveType["char"], false, true, maxLength);
              else
              {
                if (isMaxLength)
                  tu = TypeUsage.CreateStringTypeUsage(StoreTypeNameToStorePrimitiveType["varchar"], false, false);
                else
                  tu = TypeUsage.CreateStringTypeUsage(StoreTypeNameToStorePrimitiveType["varchar"], false, false, maxLength);
              }
            }
            return tu;
          }
        case PrimitiveTypeKind.DateTime: // datetime, smalldatetime
          return TypeUsage.CreateDefaultTypeUsage(StoreTypeNameToStorePrimitiveType["datetime"]);
        default:
          throw new NotSupportedException(String.Format("There is no store type corresponding to the EDM type '{0}' of primitive type '{1}'.", edmType, primitiveType.PrimitiveTypeKind));
      }
    }

    private XmlReader GetStoreSchemaMapping()
    {
      return GetXmlResource("System.Data.SQLite.SQLiteProviderServices.StoreSchemaMapping.msl");
    }

    private XmlReader GetStoreSchemaDescription()
    {
      return GetXmlResource("System.Data.SQLite.SQLiteProviderServices.StoreSchemaDefinition.ssdl");
    }

    internal static XmlReader GetXmlResource(string resourceName)
    {
      Assembly executingAssembly = Assembly.GetExecutingAssembly();
      Stream stream = executingAssembly.GetManifestResourceStream(resourceName);
      return XmlReader.Create(stream);
    }

    private static class TypeHelpers
    {
      public static bool TryGetPrecision(TypeUsage tu, out byte precision)
      {
        Facet f;

        precision = 0;
        if (tu.Facets.TryGetValue("Precision", false, out f))
        {
          if (!f.IsUnbounded && f.Value != null)
          {
            precision = (byte)f.Value;
            return true;
          }
        }
        return false;
      }

      public static bool TryGetMaxLength(TypeUsage tu, out int maxLength)
      {
        Facet f;

        maxLength = 0;
        if (tu.Facets.TryGetValue("MaxLength", false, out f))
        {
          if (!f.IsUnbounded && f.Value != null)
          {
            maxLength = (int)f.Value;
            return true;
          }
        }
        return false;
      }

      public static bool TryGetScale(TypeUsage tu, out byte scale)
      {
        Facet f;

        scale = 0;
        if (tu.Facets.TryGetValue("Scale", false, out f))
        {
          if (!f.IsUnbounded && f.Value != null)
          {
            scale = (byte)f.Value;
            return true;
          }
        }
        return false;
      }
    }
  }
}