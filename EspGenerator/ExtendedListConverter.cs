using Newtonsoft.Json;
using Noggog;

namespace EspGenerator
{
    public sealed class ExtendedListConverter : JsonConverter
    {
        public override bool CanConvert(Type objectType)
        {
            return objectType.IsGenericType && (objectType.GetGenericTypeDefinition() == typeof(ExtendedList<>) || objectType.GetGenericTypeDefinition() == typeof(IExtendedList<>));
        }

        public override object? ReadJson(JsonReader reader, Type objectType, object? existingValue, JsonSerializer serializer)
        {
            return Activator.CreateInstance(typeof(ExtendedList<>).MakeGenericType(objectType.GenericTypeArguments[0]),
                serializer.Deserialize(reader, typeof(List<>).MakeGenericType(objectType.GenericTypeArguments[0])));
        }

        public override void WriteJson(JsonWriter writer, object? value, JsonSerializer serializer)
        {
            if (value == null) return;
            writer.WriteValue(((dynamic)value).ToArray());
        }
    }
}
