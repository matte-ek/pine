using System;

namespace Pine.Core
{
    // How a script's fields are presented in the editor's properties panel.
    //
    // Only the first two change whether a field is reflected at all; the rest affect nothing but
    // the way its row is drawn. None of them touch how a value is stored, so adding or removing one
    // never invalidates an already authored scene.

    // Reflect a private field. A public field is reflected without asking.
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class SerializeFieldAttribute : Attribute
    {
    }

    // Do not reflect this field, however it is declared. The field still works in C#; it just
    // cannot be authored, and nothing about it is stored.
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class HideInInspectorAttribute : Attribute
    {
    }

    // Draw a slider between Min and Max rather than an input field. Only Float and Integer fields
    // are drawn as sliders; on any other type the attribute is ignored.
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class RangeAttribute : Attribute
    {
        public RangeAttribute(float min, float max)
        {
            Min = min;
            Max = max;
        }

        public float Min { get; }
        public float Max { get; }
    }

    // Hover text on the field's row.
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class TooltipAttribute : Attribute
    {
        public TooltipAttribute(string text)
        {
            Text = text;
        }

        public string Text { get; }
    }

    // A labelled separator above the field, for naming a group of them.
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class HeaderAttribute : Attribute
    {
        public HeaderAttribute(string text)
        {
            Text = text;
        }

        public string Text { get; }
    }

    // Blank space above the field, for separating a group without naming it.
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class SpaceAttribute : Attribute
    {
    }
}
