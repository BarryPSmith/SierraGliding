using core_Receiver.Packets;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;

namespace core_Receiver
{
    public static class Extensions
    {
        public static bool IsHex(this char c)
        {
            return (c >= '0' && c <= '9') ||
                     (c >= 'a' && c <= 'f') ||
                     (c >= 'A' && c <= 'F');
        }

        public static string ToCsv<T>(this IEnumerable<T> source, Func<T, string> transformer = null, string separator = ", ")
        {
            if (transformer == null)
                transformer = A => A.ToString();
            return source?.Aggregate("", (run, cur) => run + (string.IsNullOrEmpty(run) ? "" : separator) + transformer(cur));
        }

        public static char ToChar(this byte b)
        {
            if (b >= 32 && b < 128)
                return (char) b;
            else
                return '#';
        }

        public static string ToCharOrNumber(this byte b)
        {
            if (b >= 32 && b < 128)
                return $"{(char)b}";
            else
                return $"[{b}]";
        }

        public static bool IsPrintableChar(this byte b)
        {
            return b >= 32 && b < 128;
        }

        public static char ToChar(this PacketTypes packet)
            => ((byte)packet).ToChar();

        public static string ToAscii(this IEnumerable<byte> array,
            int start = 0)
        {
            return new string(array.Skip(start).Select(b => b.ToChar()).ToArray());
        }

        public static void Append<TKey, TValue, TList>(this Dictionary<TKey, TList> dict, TKey key, TValue value)
            where TList : ICollection<TValue>, new()
        {
            if (!dict.TryGetValue(key, out var list))
            {
                list = new TList();
                dict[key] = list;
            }
            list.Add(value);
        }
    }

}
