// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.indexinglanguage.expressions;

import com.yahoo.document.DataType;
import com.yahoo.document.Field;
import com.yahoo.document.datatypes.Array;
import com.yahoo.document.datatypes.FieldValue;
import com.yahoo.document.datatypes.IntegerFieldValue;
import com.yahoo.document.datatypes.StringFieldValue;
import com.yahoo.document.datatypes.WeightedSet;
import com.yahoo.vespa.indexinglanguage.SimpleTestAdapter;
import org.junit.Test;

import java.util.List;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

/**
 * @author Simon Thoresen Hult
 */
@SuppressWarnings({ "rawtypes" })
public class CatTestCase {

    @Test
    public void requireThatAccessorsWork() {
        Expression foo = new AttributeExpression("foo");
        Expression bar = new AttributeExpression("bar");
        CatExpression exp = new CatExpression(foo, bar);
        assertEquals(2, exp.size());
        assertSame(foo, exp.get(0));
        assertSame(bar, exp.get(1));
        assertEquals(List.of(foo, bar), exp.asList());
    }

    @Test
    public void requireThatHashCodeAndEqualsAreImplemented() {
        Expression foo = new AttributeExpression("foo");
        Expression bar = new AttributeExpression("bar");
        Expression exp = new CatExpression(foo, bar);
        assertNotEquals(exp, new Object());
        assertNotEquals(exp, new StatementExpression(foo, bar));
        assertNotEquals(exp, new CatExpression());
        assertNotEquals(exp, new CatExpression(foo));
        assertNotEquals(exp, new CatExpression(bar, foo));
        assertEquals(exp, new CatExpression(foo, bar));
        assertEquals(exp.hashCode(), new CatExpression(foo, bar).hashCode());
    }

    @Test
    public void expressionCanBeVerified() {
        assertVerify(new ConstantExpression(new StringFieldValue("foo")),
                     new ConstantExpression(new StringFieldValue("bar")), null);
        assertVerifyThrows(new SimpleExpression(DataType.STRING),
                           new SimpleExpression(DataType.STRING), null,
                           "Invalid expression 'SimpleExpression': Expected string input, but no input is provided");
        assertVerifyThrows(new SimpleExpression(DataType.STRING),
                           new SimpleExpression(DataType.STRING), DataType.INT,
                           "Invalid expression 'SimpleExpression': Expected string input, got int");
    }

    @Test
    public void requireThatPrimitivesAreConcatenated() {
        assertEquals(new StringFieldValue("69"), evaluate(new StringFieldValue("6"), new StringFieldValue("9")));
        assertEquals(new StringFieldValue("69"), evaluate(new StringFieldValue("6"), new IntegerFieldValue(9)));
        assertEquals(new StringFieldValue("69"), evaluate(new IntegerFieldValue(6), new IntegerFieldValue(9)));
        assertEquals(new StringFieldValue("69"), evaluate(new IntegerFieldValue(6), new StringFieldValue("9")));
    }

    @Test
    public void requireThatPrimitivesCanNotBeNull() {
        assertNull(evaluate(DataType.STRING, new StringFieldValue("69"), DataType.STRING, null));
        assertNull(evaluate(DataType.STRING, null, DataType.STRING, new StringFieldValue("69")));
        assertNull(evaluate(DataType.INT, new IntegerFieldValue(69), DataType.INT, null));
        assertNull(evaluate(DataType.INT, null, DataType.INT, new IntegerFieldValue(69)));
    }

    @Test
    public void inputValueIsAvailableToAllInnerExpressions() {
        var expression = new StatementExpression(new ConstantExpression(new StringFieldValue("foo")),
                                                 new CatExpression(new ThisExpression(),
                                                                   new ConstantExpression(new StringFieldValue("bar")),
                                                                   new ThisExpression()));
        expression.resolve(new SimpleTestAdapter());
        assertEquals(new StringFieldValue("foobarfoo"), expression.execute());
    }

    @Test
    public void requiredInputTypeAllowsNull() {
         assertVerify(new ConstantExpression(new StringFieldValue("foo")), new TrimExpression(), DataType.STRING);
         assertVerify(new TrimExpression(), new ConstantExpression(new StringFieldValue("foo")), DataType.STRING);
    }

    @Test
    public void arraysAreConcatenated() {
        Array<StringFieldValue> lhs = new Array<>(DataType.getArray(DataType.STRING));
        lhs.add(new StringFieldValue("6"));
        Array<StringFieldValue> rhs = new Array<>(DataType.getArray(DataType.STRING));
        rhs.add(new StringFieldValue("9"));

        FieldValue val = evaluate(lhs, rhs);
        assertTrue(val instanceof Array);

        Array arr = (Array)val;
        assertEquals(2, arr.size());
        assertEquals(new StringFieldValue("6"), arr.get(0));
        assertEquals(new StringFieldValue("9"), arr.get(1));
    }

    @Test
    public void requireThatArraysCanBeNull() {
        DataType type = DataType.getArray(DataType.STRING);
        Array<StringFieldValue> arr = new Array<>(type);
        arr.add(new StringFieldValue("9"));

        FieldValue val = evaluate(type, null, type, arr);
        assertEquals(type, val.getDataType());
        assertEquals(1, ((Array)val).size());
        assertEquals(new StringFieldValue("9"), ((Array)val).get(0));

        val = evaluate(type, arr, type, null);
        assertEquals(type, val.getDataType());
        assertEquals(1, ((Array)val).size());
        assertEquals(new StringFieldValue("9"), ((Array)val).get(0));
    }

    @Test
    public void requireThatWsetsAreConcatenated() {
        WeightedSet<StringFieldValue> lhs = new WeightedSet<>(DataType.getWeightedSet(DataType.STRING));
        lhs.put(new StringFieldValue("6"), 9);
        WeightedSet<StringFieldValue> rhs = new WeightedSet<>(DataType.getWeightedSet(DataType.STRING));
        rhs.put(new StringFieldValue("9"), 6);

        FieldValue val = evaluate(lhs, rhs);
        assertTrue(val instanceof WeightedSet);

        WeightedSet wset = (WeightedSet)val;
        assertEquals(2, wset.size());
        assertEquals(Integer.valueOf(9), wset.get(new StringFieldValue("6")));
        assertEquals(Integer.valueOf(6), wset.get(new StringFieldValue("9")));
    }

    @Test
    public void requireThatWsetsCanBeNull() {
        DataType type = DataType.getWeightedSet(DataType.STRING);
        WeightedSet<StringFieldValue> wset = new WeightedSet<>(type);
        wset.put(new StringFieldValue("6"), 9);

        FieldValue val = evaluate(type, null, type, wset);
        assertEquals(type, val.getDataType());
        assertEquals(1, ((WeightedSet)val).size());
        assertEquals(Integer.valueOf(9), ((WeightedSet)val).get(new StringFieldValue("6")));

        val = evaluate(type, wset, type, null);
        assertEquals(type, val.getDataType());
        assertEquals(1, ((WeightedSet)val).size());
        assertEquals(Integer.valueOf(9), ((WeightedSet)val).get(new StringFieldValue("6")));
    }

    @Test
    public void requireThatCollectionValuesMustBeCompatible() {
        {
            Array<StringFieldValue> arrA = new Array<>(DataType.getArray(DataType.STRING));
            arrA.add(new StringFieldValue("6"));
            Array<IntegerFieldValue> arrB = new Array<>(DataType.getArray(DataType.INT));
            arrB.add(new IntegerFieldValue(9));
            assertEquals(new StringFieldValue(arrA.toString() + arrB.toString()), evaluate(arrA, arrB));
            assertEquals(new StringFieldValue(arrB.toString() + arrA.toString()), evaluate(arrB, arrA));
        }
        {
            Array<StringFieldValue> arr = new Array<>(DataType.getArray(DataType.STRING));
            arr.add(new StringFieldValue("6"));
            WeightedSet<StringFieldValue> wset = new WeightedSet<>(DataType.getWeightedSet(DataType.STRING));
            wset.add(new StringFieldValue("9"));
            assertEquals(new StringFieldValue(arr.toString() + wset.toString()), evaluate(arr, wset));
            assertEquals(new StringFieldValue(wset.toString() + arr.toString()), evaluate(wset, arr));
        }
        {
            WeightedSet<StringFieldValue> wsetA = new WeightedSet<>(DataType.getWeightedSet(DataType.STRING));
            wsetA.add(new StringFieldValue("6"));
            WeightedSet<IntegerFieldValue> wsetB = new WeightedSet<>(DataType.getWeightedSet(DataType.INT));
            wsetB.add(new IntegerFieldValue(9));
            assertEquals(new StringFieldValue(wsetA.toString() + wsetB.toString()), evaluate(wsetA, wsetB));
            assertEquals(new StringFieldValue(wsetB.toString() + wsetA.toString()), evaluate(wsetB, wsetA));
        }
    }

    private static void assertVerify(Expression expA, Expression expB, DataType val) {
        new CatExpression(expA, expB).resolve(new TypeContext(new SimpleTestAdapter()));
    }

    private static void assertVerifyThrows(Expression expA, Expression expB, DataType val, String expectedException) {
        try {
            var expression = new CatExpression(expA, expB);
            var context = new TypeContext(new SimpleTestAdapter());
            expression.setInputType(val, context);
            expression.resolve(context);
            fail("Expected exception");
        } catch (VerificationException e) {
            if (!e.getMessage().startsWith(expectedException)) {
                assertEquals(expectedException, e.getMessage());
            }
        }
    }

    private static FieldValue evaluate(FieldValue valA, FieldValue valB) {
        return evaluate(valA.getDataType(), valA, valB.getDataType(), valB);
    }

    private static FieldValue evaluate(DataType typeA, FieldValue valA, DataType typeB, FieldValue valB) {
        var adapter = new SimpleTestAdapter(new Field("a", typeA),
                                            new Field("b", typeB));
        var expression = new CatExpression(new InputExpression("a"), new InputExpression("b"));
        expression.setInputType(null, new TypeContext(adapter));
        ExecutionContext context = new ExecutionContext(adapter);
        context.setFieldValue("a", valA, null);
        context.setFieldValue("b", valB, null);
        expression.execute(context);
        return context.getCurrentValue();
    }

}
