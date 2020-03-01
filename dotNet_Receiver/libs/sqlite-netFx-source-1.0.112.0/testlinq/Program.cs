/********************************************************
 * ADO.NET 2.0 Data Provider for SQLite Version 3.X
 * Written by Robert Simpson (robert@blackcastlesoft.com)
 *
 * Released to the public domain, use at your own risk!
 ********************************************************/

using System;
using System.Collections.Generic;
using System.Data;
using System.Data.Common;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Transactions;

#if USE_ENTITY_FRAMEWORK_6
using System.Data.Entity.Core;
using System.Data.Entity.Core.EntityClient;
using System.Data.Entity.Core.Objects;
#else
using System.Data.EntityClient;
using System.Data.Objects;
#endif

#if NET_STANDARD_21
using System.Data.SQLite.EF6;
#endif

namespace testlinq
{
  class Program
  {
      private static int Main(string[] args)
      {
          if (Environment.GetEnvironmentVariable("BREAK") != null)
          {
              Console.WriteLine(
                  "Attach a debugger to process {0} and press any key to continue.",
                  Process.GetCurrentProcess().Id);

              try
              {
                  Console.ReadKey(true); /* throw */
              }
              catch (InvalidOperationException) // Console.ReadKey
              {
                  // do nothing.
              }

              Debugger.Break();
          }

#if NET_STANDARD_21
          DbProviderFactories.RegisterFactory("System.Data.SQLite.EF6", SQLiteProviderFactory.Instance);
#endif

          string arg = null;

          if ((args != null) && (args.Length > 0))
              arg = args[0];

          if (arg == null)
              arg = String.Empty;

          arg = arg.Trim().TrimStart('-', '/').ToLowerInvariant();

          switch (arg)
          {
              case "": // String.Empty
              case "old":
                  {
                      return OldTests();
                  }
              case "datetime":
                  {
                      return DateTimeTest();
                  }
              case "datetime2":
                  {
                      string dateTimeFormat = null;

                      if (args.Length > 1)
                          dateTimeFormat = args[1];

                      DateTimeTest2(dateTimeFormat);
                      return 0;
                  }
              case "skip":
                  {
                      int pageSize = 0;

                      if (args.Length > 1)
                      {
                          arg = args[1];

                          if (arg != null)
                              pageSize = int.Parse(arg.Trim());
                      }

                      return SkipTest(pageSize);
                  }
              case "substring":
                  {
                      return SubStringTest();
                  }
#if USE_INTEROP_DLL && INTEROP_EXTENSION_FUNCTIONS
              case "unionall":
                  {
                      return UnionAllTest();
                  }
#endif
              case "endswith":
                  {
                      string value = null;

                      if (args.Length > 1)
                      {
                          value = args[1];

                          if (value != null)
                              value = value.Trim();
                      }

                      return EndsWithTest(value);
                  }
              case "startswith":
                  {
                      string value = null;

                      if (args.Length > 1)
                      {
                          value = args[1];

                          if (value != null)
                              value = value.Trim();
                      }

                      return StartsWithTest(value);
                  }
              case "eftransaction":
                  {
                      bool value = false;

                      if (args.Length > 1)
                      {
                          if (!bool.TryParse(args[1], out value))
                          {
                              Console.WriteLine(
                                  "cannot parse \"{0}\" as boolean",
                                  args[1]);

                              return 1;
                          }
                      }

                      return EFTransactionTest(value);
                  }
              case "insert":
                  {
                      return InsertTest();
                  }
              case "update":
                  {
                      return UpdateTest();
                  }
              case "binaryguid":
                  {
                      bool value = false;

                      if (args.Length > 1)
                      {
                          if (!bool.TryParse(args[1], out value))
                          {
                              Console.WriteLine(
                                  "cannot parse \"{0}\" as boolean",
                                  args[1]);

                              return 1;
                          }
                      }

                      return BinaryGuidTest(value);
                  }
#if NET_40 || NET_45 || NET_451 || NET_452 || NET_46 || NET_461 || NET_462 || NET_47 || NET_471 || NET_472 || NET_STANDARD_21
              case "binaryguid2":
                  {
                      bool value = false;

                      if (args.Length > 1)
                      {
                          if (!bool.TryParse(args[1], out value))
                          {
                              Console.WriteLine(
                                  "cannot parse \"{0}\" as boolean",
                                  args[1]);

                              return 1;
                          }
                      }

                      return BinaryGuidTest2(value);
                  }
#endif
#if NET_40 || NET_45 || NET_451 || NET_452 || NET_46 || NET_461 || NET_462 || NET_47 || NET_471 || NET_472 || NET_STANDARD_21
              case "round":
                  {
                      return RoundTest();
                  }
#endif
              case "complexprimarykey":
                  {
                      return ComplexPrimaryKeyTest();
                  }
              default:
                  {
                      Console.WriteLine("unknown test \"{0}\"", arg);
                      return 1;
                  }
          }
      }

      /// <summary>
      /// Attempts to obtain the underlying store connection
      /// (a <see cref="DbConnection" />) from the specified
      /// <see cref="EntityConnection" />.
      /// </summary>
      /// <param name="entityConnection">
      /// The <see cref="EntityConnection" /> to use.
      /// </param>
      /// <returns>
      /// The <see cref="DbConnection" /> -OR- null if it
      /// cannot be determined.
      /// </returns>
      private static DbConnection GetStoreConnection(
          EntityConnection entityConnection
          )
      {
          //
          // NOTE: No entity connection, no store connection.
          //
          if (entityConnection == null)
              return null;

          //
          // HACK: We need the underlying store connection and
          //       the legacy versions of the .NET Framework do
          //       not expose it; therefore, attempt to grab it
          //       by force.
          //
          FieldInfo fieldInfo = typeof(EntityConnection).GetField(
              "_storeConnection", BindingFlags.Instance |
              BindingFlags.NonPublic);

          //
          // NOTE: If the field is not found, just return null.
          //
          if (fieldInfo == null)
              return null;

          return fieldInfo.GetValue(entityConnection) as DbConnection;
      }

      //
      // NOTE: Used to verify that the SUBSTR function is used to
      //       implement the Substring method.
      //
      private static int SubStringTest()
      {
          using (northwindEFEntities db = new northwindEFEntities())
          {
              try
              {
                  bool once = false;

                  var query = db.Customers.Select(
                      p => "test".Substring(1) != null);

                  foreach (var result in query)
                  {
                      if (once)
                          Console.Write(' ');

                      Console.Write(result);

                      once = true;
                  }

                  return 0;
              }
              catch (Exception e)
              {
                  Console.WriteLine(e);
              }
          }

          return 1;
      }

      //
      // NOTE: Used to test the fix for ticket [8b7d179c3c].
      //
      private static int SkipTest(int pageSize)
      {
          using (northwindEFEntities db = new northwindEFEntities())
          {
              bool once = false;
              int count = db.Customers.Count();

              int PageCount = (pageSize != 0) ?
                  (count / pageSize) + ((count % pageSize) == 0 ? 0 : 1) : 1;

              for (int pageIndex = 0; pageIndex < PageCount; pageIndex++)
              {
                  var query = db.Customers.OrderBy(p => p.City).
                      Skip(pageSize * pageIndex).Take(pageSize);

                  foreach (Customers customers in query)
                  {
                      if (once)
                          Console.Write(' ');

                      Console.Write(customers.CustomerID);

                      once = true;
                  }
              }
          }

          return 0;
      }

      //
      // NOTE: Used to test the fix for ticket [59edc1018b].
      //
      private static int EndsWithTest(string value)
      {
          using (northwindEFEntities db = new northwindEFEntities())
          {
              bool once = false;
              var query = from c in db.Customers
                          where c.City.EndsWith(value)
                          orderby c.CustomerID
                          select c;

              foreach (Customers customers in query)
              {
                  if (once)
                      Console.Write(' ');

                  Console.Write(customers.CustomerID);

                  once = true;
              }
          }

          return 0;
      }

      //
      // NOTE: Used to verify the behavior from ticket [00f86f9739].
      //
      private static int StartsWithTest(string value)
      {
          using (northwindEFEntities db = new northwindEFEntities())
          {
              bool once = false;
              var query = from c in db.Customers
                          where c.City.StartsWith(value)
                          orderby c.CustomerID
                          select c;

              foreach (Customers customers in query)
              {
                  if (once)
                      Console.Write(' ');

                  Console.Write(customers.CustomerID);

                  once = true;
              }
          }

          return 0;
      }

#if USE_INTEROP_DLL && INTEROP_EXTENSION_FUNCTIONS
      //
      // NOTE: Used to test the fix for ticket [0a32885109].
      //
      private static int UnionAllTest()
      {
          using (northwindEFEntities db = new northwindEFEntities())
          {
              bool once = false;

              var customers1 = db.Customers.Where(
                  f => f.Orders.Any()).OrderByDescending(
                    f => f.CompanyName).Skip(1).Take(1);

              var customers2 = db.Customers.Where(
                  f => f.Orders.Any()).OrderBy(
                    f => f.CompanyName).Skip(1).Take(1);

              var customers3 = db.Customers.Where(
                  f => f.CustomerID.StartsWith("B")).OrderBy(
                    f => f.CompanyName).Skip(1).Take(1);

              foreach (var customer in customers1)
              {
                  if (once)
                      Console.Write(' ');

                  Console.Write(customer.CustomerID);
                  once = true;
              }

              foreach (var customer in customers2)
              {
                  if (once)
                      Console.Write(' ');

                  Console.Write(customer.CustomerID);
                  once = true;
              }

              foreach (var customer in customers3)
              {
                  if (once)
                      Console.Write(' ');

                  Console.Write(customer.CustomerID);
                  once = true;
              }

              foreach (var customer in customers1.Concat(customers2))
              {
                  if (once)
                      Console.Write(' ');

                  Console.Write(customer.CustomerID);
                  once = true;
              }

              foreach (var customer in
                    customers1.Concat(customers2).Concat(customers3))
              {
                  if (once)
                      Console.Write(' ');

                  Console.Write(customer.CustomerID);
                  once = true;
              }
          }

          return 0;
      }
#endif

      //
      // NOTE: Used to test the fix for ticket [ccfa69fc32].
      //
      private static int EFTransactionTest(bool add)
      {
          //
          // NOTE: Some of these territories already exist and should cause
          //       an exception to be thrown when we try to INSERT them.
          //
          long[] territoryIds = new long[] {
                 1,    2,    3,    4,    5, // NOTE: Success
                 6,    7,    8,    9,   10, // NOTE: Success
              1576, 1577, 1578, 1579, 1580, // NOTE: Success
              1581, 1730, 1833, 2116, 2139, // NOTE: Fail (1581)
              2140, 2141                    // NOTE: Skipped
          };

          if (add)
          {
              using (northwindEFEntities db = new northwindEFEntities())
              {
                  using (TransactionScope scope = new TransactionScope())
                  {
                      //
                      // NOTE: *REQUIRED* This is required so that the
                      //       Entity Framework is prevented from opening
                      //       multiple connections to the underlying SQLite
                      //       database (i.e. which would result in multiple
                      //       IMMEDIATE transactions, thereby failing [later
                      //       on] with locking errors).
                      //
                      db.Connection.Open();

                      foreach (int id in territoryIds)
                      {
                          Territories territories = new Territories();

                          territories.TerritoryID = id;
                          territories.TerritoryDescription = String.Format(
                              "Test Territory #{0}", id);
                          territories.Regions = db.Regions.First();

                          db.AddObject("Territories", territories);
                      }

                      try
                      {
#if NET_40 || NET_45 || NET_451 || NET_452 || NET_46 || NET_461 || NET_462 || NET_47 || NET_471 || NET_472 || NET_STANDARD_21
                          db.SaveChanges(SaveOptions.None);
#else
                          db.SaveChanges(false);
#endif
                      }
                      catch (Exception e)
                      {
                          Console.WriteLine(e);
                      }
                      finally
                      {
                          scope.Complete();
                          db.AcceptAllChanges();
                      }
                  }
              }
          }
          else
          {
              using (northwindEFEntities db = new northwindEFEntities())
              {
                  bool once = false;
#if NET_40 || NET_45 || NET_451 || NET_452 || NET_46 || NET_461 || NET_462 || NET_47 || NET_471 || NET_472 || NET_STANDARD_21
                  var query = from t in db.Territories
                    where territoryIds.AsQueryable<long>().Contains<long>(t.TerritoryID)
                    orderby t.TerritoryID
                    select t;

                  foreach (Territories territories in query)
                  {
                      if (once)
                          Console.Write(' ');

                      Console.Write(territories.TerritoryID);

                      once = true;
                  }
#else
                  //
                  // HACK: We cannot use the Contains extension method within a
                  //       LINQ query with the .NET Framework 3.5.
                  //
                  var query = from t in db.Territories
                    orderby t.TerritoryID
                    select t;

                  foreach (Territories territories in query)
                  {
                      if (Array.IndexOf(territoryIds, territories.TerritoryID) == -1)
                          continue;

                      if (once)
                          Console.Write(' ');

                      Console.Write(territories.TerritoryID);

                      once = true;
                  }
#endif
              }
          }

          return 0;
      }

      //
      // NOTE: Used to test the INSERT fix (i.e. an extra semi-colon in
      //       the SQL statement after the actual INSERT statement in
      //       the follow-up SELECT statement).
      //
      private static int InsertTest()
      {
          using (northwindEFEntities db = new northwindEFEntities())
          {
              long orderId = 10248;
              long productId = 1;
              int[] counts = { 0 };

              //
              // NOTE: *REQUIRED* This is required so that the
              //       Entity Framework is prevented from opening
              //       multiple connections to the underlying SQLite
              //       database (i.e. which would result in multiple
              //       IMMEDIATE transactions, thereby failing [later
              //       on] with locking errors).
              //
              db.Connection.Open();

              KeyValuePair<string, object> orderIdPair =
                  new KeyValuePair<string, object>("OrderID", orderId);

              KeyValuePair<string, object> productIdPair =
                  new KeyValuePair<string, object>("ProductID", productId);

              /////////////////////////////////////////////////////////////////

              OrderDetails newOrderDetails = new OrderDetails();

              newOrderDetails.OrderID = orderId;
              newOrderDetails.ProductID = productId;
              newOrderDetails.UnitPrice = (decimal)1.23;
              newOrderDetails.Quantity = 1;
              newOrderDetails.Discount = 0.0f;

              newOrderDetails.OrdersReference.EntityKey = new EntityKey(
                  "northwindEFEntities.Orders",
                  new KeyValuePair<string, object>[] { orderIdPair });

              newOrderDetails.ProductsReference.EntityKey = new EntityKey(
                  "northwindEFEntities.Products",
                  new KeyValuePair<string, object>[] { productIdPair });

              db.AddObject("OrderDetails", newOrderDetails);

              try
              {
                  db.SaveChanges();
                  counts[0]++;
              }
              catch (Exception e)
              {
                  Console.WriteLine(e);
              }
              finally
              {
                  db.AcceptAllChanges();
              }

              Console.WriteLine("inserted {0}", counts[0]);
          }

          return 0;
      }

      //
      // NOTE: Used to test the UPDATE fix (i.e. the missing semi-colon
      //       in the SQL statement between the actual UPDATE statement
      //       and the follow-up SELECT statement).
      //
      private static int UpdateTest()
      {
          long[] orderIds = new long[] {
              0
          };

          using (northwindEFEntities db = new northwindEFEntities())
          {
              int[] counts = { 0, 0 };

              //
              // NOTE: *REQUIRED* This is required so that the
              //       Entity Framework is prevented from opening
              //       multiple connections to the underlying SQLite
              //       database (i.e. which would result in multiple
              //       IMMEDIATE transactions, thereby failing [later
              //       on] with locking errors).
              //
              db.Connection.Open();

              for (int index = 0; index < orderIds.Length; index++)
              {
                  Orders newOrders = new Orders();

                  newOrders.ShipAddress = String.Format(
                      "Test Order Ship Address, Index #{0}",
                      index);

                  db.AddObject("Orders", newOrders);

                  try
                  {
                      db.SaveChanges();
                      counts[0]++;

                      // StoreGeneratedPattern="Identity"
                      orderIds[index] = newOrders.OrderID;

                      // StoreGeneratedPattern="None"
                      newOrders.ShipAddress = String.Format(
                          "New Order Ship Address #{0}",
                          orderIds[index]);

                      // StoreGeneratedPattern="Computed"
                      newOrders.Freight = 1;

                      db.SaveChanges();
                      counts[1]++;
                  }
                  catch (Exception e)
                  {
                      Console.WriteLine(e);
                  }
                  finally
                  {
                      db.AcceptAllChanges();
                  }
              }

              Console.WriteLine(
                  "inserted {0} updated {1}", counts[0], counts[1]);
          }

          return 0;
      }

      //
      // NOTE: Used to test the BinaryGUID fix (i.e. BLOB literal formatting
      //       of GUID values when the BinaryGUID connection property has been
      //       enabled).
      //
      private static int BinaryGuidTest(bool binaryGuid)
      {
          Environment.SetEnvironmentVariable(
              "AppendManifestToken_SQLiteProviderManifest",
              String.Format(";BinaryGUID={0};", binaryGuid));

          using (northwindEFEntities db = new northwindEFEntities())
          {
              string sql = "SELECT VALUE GUID " +
                  "'2d3d2d3d-2d3d-2d3d-2d3d-2d3d2d3d2d3d' " +
                  "FROM Orders AS o WHERE o.OrderID = 10248;";

              ObjectQuery<string> query = db.CreateQuery<string>(sql);

              foreach (string s in query)
                  Console.WriteLine(s);
          }

          Environment.SetEnvironmentVariable(
              "AppendManifestToken_SQLiteProviderManifest",
              null);

          return 0;
      }

#if NET_40 || NET_45 || NET_451 || NET_452 || NET_46 || NET_461 || NET_462 || NET_47 || NET_471 || NET_472 || NET_STANDARD_21
      //
      // NOTE: Used to test the BinaryGUID connection string property with
      //       the Contains() function (ticket [a4d9c7ee94]).  We cannot
      //       use the Contains extension method within a LINQ query with
      //       the .NET Framework 3.5.
      //
      private static int BinaryGuidTest2(bool binaryGuid)
      {
          Environment.SetEnvironmentVariable(
              "AppendManifestToken_SQLiteProviderManifest",
              String.Format(";BinaryGUID={0};", binaryGuid));

          using (northwindEFEntities db = new northwindEFEntities())
          {
              Guid guid = new Guid("2d3d2d3d-2d3d-2d3d-2d3d-2d3d2d3d2d3d");
              Guid[] guids = new Guid[] { guid };

              bool once = false;
              var query = from t in db.Territories
                          where guids.AsQueryable<Guid>().Contains<Guid>(guid)
                          orderby t.TerritoryID
                          select t;

              foreach (Territories t in query)
              {
                  if (once)
                      Console.Write(' ');

                  Console.Write(t.TerritoryID);

                  once = true;
              }
          }

          Environment.SetEnvironmentVariable(
              "AppendManifestToken_SQLiteProviderManifest",
              null);

          return 0;
      }
#endif

      private static int ComplexPrimaryKeyTest()
      {
          using (northwindEFEntities db = new northwindEFEntities())
          {
              long orderId = 10248;
              long productId = 1;
              int[] counts = { 0, 0 };

              //
              // NOTE: *REQUIRED* This is required so that the
              //       Entity Framework is prevented from opening
              //       multiple connections to the underlying SQLite
              //       database (i.e. which would result in multiple
              //       IMMEDIATE transactions, thereby failing [later
              //       on] with locking errors).
              //
              db.Connection.Open();

              KeyValuePair<string, object> orderIdPair =
                  new KeyValuePair<string, object>("OrderID", orderId);

              KeyValuePair<string, object> productIdPair =
                  new KeyValuePair<string, object>("ProductID", productId);

              /////////////////////////////////////////////////////////////////

              OrderDetails newOrderDetails = new OrderDetails();

              newOrderDetails.OrderID = orderId;
              newOrderDetails.ProductID = productId;
              newOrderDetails.UnitPrice = (decimal)1.23;
              newOrderDetails.Quantity = 1;
              newOrderDetails.Discount = 0.0f;

              newOrderDetails.OrdersReference.EntityKey = new EntityKey(
                  "northwindEFEntities.Orders",
                  new KeyValuePair<string, object>[] { orderIdPair });

              newOrderDetails.ProductsReference.EntityKey = new EntityKey(
                  "northwindEFEntities.Products",
                  new KeyValuePair<string, object>[] { productIdPair });

              db.AddObject("OrderDetails", newOrderDetails);

              try
              {
                  db.SaveChanges();
                  counts[0]++;
              }
              catch (Exception e)
              {
                  Console.WriteLine(e);
              }
              finally
              {
                  db.AcceptAllChanges();
              }

              try
              {
                  db.Refresh(RefreshMode.StoreWins, newOrderDetails);
                  counts[0]++;
              }
              catch (Exception e)
              {
                  Console.WriteLine(e);
              }

              Console.WriteLine("inserted {0}", counts[0]);

              /////////////////////////////////////////////////////////////////

              newOrderDetails.UnitPrice = (decimal)2.34;
              newOrderDetails.Quantity = 2;
              newOrderDetails.Discount = 0.1f;

              try
              {
                  db.SaveChanges();
                  counts[1]++;
              }
              catch (Exception e)
              {
                  Console.WriteLine(e);
              }
              finally
              {
                  db.AcceptAllChanges();
              }

              try
              {
                  db.Refresh(RefreshMode.StoreWins, newOrderDetails);
                  counts[1]++;
              }
              catch (Exception e)
              {
                  Console.WriteLine(e);
              }

              Console.WriteLine("updated {0}", counts[1]);
          }

          return 0;
      }

      private static int DateTimeTest()
      {
          using (northwindEFEntities db = new northwindEFEntities())
          {
              DateTime dateTime = new DateTime(1997, 1, 1, 0, 0, 0, DateTimeKind.Local);
              int c1 = db.Orders.Where(i => i.OrderDate == new DateTime(1997, 1, 1, 0, 0, 0, DateTimeKind.Local)).Count();
              int c2 = db.Orders.Where(i => i.OrderDate == dateTime).Count();
              return c1 == c2 ? 0 : 1;
          }
      }

      private static void DateTimeTest2(
          string dateTimeFormat
          )
      {
#if !NET_STANDARD_20 && !NET_STANDARD_21
          TraceListener listener = new ConsoleTraceListener();
#else
          TraceListener listener = new TextWriterTraceListener(Console.Out);
#endif

          Trace.Listeners.Add(listener);
          Environment.SetEnvironmentVariable("SQLite_ForceLogPrepare", "1");

          if (dateTimeFormat != null)
          {
              Environment.SetEnvironmentVariable(
                  "AppendManifestToken_SQLiteProviderManifest",
                  String.Format(";DateTimeFormat={0};", dateTimeFormat));
          }

          using (northwindEFEntities db = new northwindEFEntities())
          {
              db.Orders.Where(i => i.OrderDate <
                  new DateTime(1997, 1, 1, 0, 0, 0, DateTimeKind.Local)).Count();
          }

          if (dateTimeFormat != null)
          {
              Environment.SetEnvironmentVariable(
                  "AppendManifestToken_SQLiteProviderManifest",
                  null);
          }

          Environment.SetEnvironmentVariable("SQLite_ForceLogPrepare", null);
          Trace.Listeners.Remove(listener);
      }

#if NET_40 || NET_45 || NET_451 || NET_452 || NET_46 || NET_461 || NET_462 || NET_47 || NET_471 || NET_472 || NET_STANDARD_21
      //
      // NOTE: Used to test the ROUND fix (i.e. being able to properly handle
      //       the two argument form).
      //
      private static int RoundTest()
      {
            using (northwindEFEntities db = new northwindEFEntities())
            {
                bool once = false;

                foreach (int[] i in new int[][] {
                        new int[] { 10503, 65 },
                        new int[] { 10503, 14 },
                        new int[] { 10635, 5 }
                    })
                {
                    for (int j = 0; j < 3; j++)
                    {
                        int oid = i[0];
                        int pid = i[1];

                        var query = from o in db.OrderDetails
                                    where o.OrderID == oid && o.ProductID == pid
                                    select new { NewUnitPrice = Math.Round(o.UnitPrice, j) };

                        foreach (object o in query)
                        {
                            if (once)
                                Console.Write(' ');

                            Console.Write("{0}", o);

                            once = true;
                        }
                    }
                }
            }

            return 0;
      }
#endif

    private static int OldTests()
    {
      using (northwindEFEntities db = new northwindEFEntities())
      {
        {
          string entitySQL = "SELECT VALUE o FROM Orders AS o WHERE SQLite.DatePart('yyyy', o.OrderDate) = 1997 ORDER BY o.OrderID;";
          ObjectQuery<Orders> query = db.CreateQuery<Orders>(entitySQL);

          foreach (Orders o in query)
          {
            Console.WriteLine(o.ShipPostalCode);
          }
        }

        {
          var query = from c in db.Customers
                      where c.City == "London"
                      orderby c.CompanyName
                      select c;

          int cc = query.Count();

          foreach (Customers c in query)
          {
            Console.WriteLine(c.CompanyName);
          }
        }

        {
          string scity = "London";
          Customers c = db.Customers.FirstOrDefault(cd => cd.City == scity);
          Console.WriteLine(c.CompanyName);
        }

        {
          DateTime dt = new DateTime(1997, 1, 1);
          var query = from order in db.Orders
                      where order.OrderDate < dt
                      orderby order.OrderID
                      select order;

          foreach (Orders o in query)
          {
            Console.WriteLine(o.OrderDate.ToString());
          }
        }

        {
          Categories c = new Categories();
          c.CategoryName = "Test Category";
          c.Description = "My Description";
          db.AddToCategories(c);
          db.SaveChanges();

          Console.WriteLine(c.CategoryID);

          c.Description = "My modified description";
          db.SaveChanges();

          db.DeleteObject(c);
          db.SaveChanges();
        }

        {
          Customers cust = new Customers();
          cust.CustomerID = "MTMTM";
          cust.ContactName = "My Name";
          cust.CompanyName = "SQLite Company";
          cust.Country = "Netherlands";
          cust.City = "Amsterdam";
          cust.Phone = "012345677";
          db.AddToCustomers(cust);
          db.SaveChanges();

          db.DeleteObject(cust);
          db.SaveChanges();
        }

        {
          var query = db.Customers.Where(cust => cust.Country == "Denmark")
                          .SelectMany(cust => cust.Orders.Where(o => o.Freight > 5))
                          .OrderBy(o => o.Customers.CustomerID);

          foreach (Orders c in query)
          {
            Console.WriteLine(c.Freight);
          }
        }

        {
          var query = from c in db.Customers
                      where c.Orders.Any(o => o.OrderDate.HasValue == true && o.OrderDate.Value.Year == 1997)
                      orderby c.CustomerID
                      select c;

          foreach (Customers c in query)
          {
            Console.WriteLine(c.CompanyName);
          }
        }

        {
          string entitySQL = "SELECT VALUE o FROM Orders AS o WHERE o.Customers.Country <> 'UK' AND o.Customers.Country <> 'Mexico' AND Year(o.OrderDate) = 1997 ORDER BY o.OrderID;";
          ObjectQuery<Orders> query = db.CreateQuery<Orders>(entitySQL);

          foreach (Orders o in query)
          {
            Console.WriteLine(o.ShipPostalCode);
          }
        }

        {
          string entitySQL = "SELECT VALUE o FROM Orders AS o WHERE NewGuid() <> NewGuid() ORDER BY o.OrderID;";
          ObjectQuery<Orders> query = db.CreateQuery<Orders>(entitySQL);

          foreach (Orders o in query)
          {
            Console.WriteLine(o.ShipPostalCode);
          }
        }

        // This query requires SQLite 3.6.2 to function correctly
        {
          var query = from p in db.Products
                      where p.OrderDetails.Count(od => od.Orders.Customers.Country == p.Suppliers.Country) > 2
                      orderby p.ProductID
                      select p;

          foreach (Products p in query)
          {
            Console.WriteLine(p.ProductName);
          }
        }
      }

      //
      // NOTE: (JJM) Removed on 2011/07/06, makes it harder to run this EXE via
      //       the new unit test suite.
      //
      // Console.ReadKey();

      return 0;
    }
  }
}
