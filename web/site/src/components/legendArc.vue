<template>
    <path :d="arc_geom" :fill="fill" />
</template>

<script>
export default {
    name: 'legendArc',

    props: [ 'fill', 'min', 'max' ],

    computed: {
        arc_geom: function() {
           return this.describeArc(50, 50, 50, this.min, this.max);
        }
    },

    methods: {

        // These two functions from https://stackoverflow.com/questions/5736398/how-to-calculate-the-svg-path-for-an-arc-of-a-circle
        polarToCartesian: function(centerX, centerY, radius, angleInDegrees) {
            var angleInRadians = (angleInDegrees-90) * Math.PI / 180.0;

            return {
            x: centerX + (radius * Math.cos(angleInRadians)),
            y: centerY + (radius * Math.sin(angleInRadians))
            };
        },

        describeArc: function (x, y, radius, startAngle, endAngle) {
            var start = this.polarToCartesian(x, y, radius, endAngle);
            var end = this.polarToCartesian(x, y, radius, startAngle);

            var largeArcFlag = endAngle - startAngle <= 180 ? "0" : "1";

            var d = [
                "M", x, y,
                "L", start.x, start.y, 
                "A", radius, radius, 0, largeArcFlag, 0, end.x, end.y,
                "Z"
            ].join(" ");

            return d;       
        },
    }
}
</script>