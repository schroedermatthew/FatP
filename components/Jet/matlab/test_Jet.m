function test_Jet
    % TEST_JET  Validate the Jet AD scalar against finite differences.
    %   Run from the folder containing Jet.m:  >> test_Jet
    %   Prints each check and errors if any fails.

    pass = true;
    h = 1e-6;
    tol = 1e-6;
    fprintf('=== Jet unit tests ===\n');

    % --- unary functions: same handle dispatches Jet (AD) and double (FD) ----
    funcs = {@sin, @cos, @tan, @exp, @log, @sqrt, @asin, @acos, @atan, @abs};
    x0s   = [0.7,  0.7,  0.7,  0.7,  2.3,  2.3,   0.4,   0.4,   0.7,   1.7];
    fprintf('-- unary derivatives --\n');
    for i = 1:numel(funcs)
        g  = funcs{i};
        x0 = x0s(i);
        y  = g(Jet.seed(x0, 1, 1));
        ad = y.d(1);
        fd = (g(x0 + h) - g(x0 - h)) / (2 * h);
        report(func2str(g), ad, fd);
    end

    % --- atan / atan2 overflow guards (FD underflows; check finiteness) ------
    fprintf('-- overflow guards --\n');
    ya = atan(Jet.seed(1e155, 1, 1));
    note('atan(1e155)', ya.d(1), isfinite(ya.d(1)) && ya.d(1) > 0, '~1e-310, finite, > 0');
    yb = atan2(Jet.seed(1e308, 1, 2), Jet.seed(1e308, 2, 2));
    note('atan2(1e308,1e308) d/dy', yb.d(1), isfinite(yb.d(1)) && yb.d(1) > 0, '~5e-309, finite, > 0');

    % --- hypot / atan2 (two variables) ---------------------------------------
    fprintf('-- two-argument functions --\n');
    yv = 1.3; xv = 2.1;
    yj = hypot(Jet.seed(yv, 1, 2), Jet.seed(xv, 2, 2));
    report('hypot d/dy', yj.d(1), (hypot(yv + h, xv) - hypot(yv - h, xv)) / (2 * h));
    report('hypot d/dx', yj.d(2), (hypot(yv, xv + h) - hypot(yv, xv - h)) / (2 * h));
    yj = atan2(Jet.seed(yv, 1, 2), Jet.seed(xv, 2, 2));
    report('atan2 d/dy', yj.d(1), (atan2(yv + h, xv) - atan2(yv - h, xv)) / (2 * h));
    report('atan2 d/dx', yj.d(2), (atan2(yv, xv + h) - atan2(yv, xv - h)) / (2 * h));

    % --- power variants ------------------------------------------------------
    fprintf('-- powers --\n');
    yj = Jet.seed(1.7, 1, 1) .^ 3;
    report('pow(x,3)', yj.d(1), ((1.7 + h) ^ 3 - (1.7 - h) ^ 3) / (2 * h));
    yj = 2 .^ Jet.seed(1.3, 1, 1);
    report('pow(2,y)', yj.d(1), (2 ^ (1.3 + h) - 2 ^ (1.3 - h)) / (2 * h));
    bv = 1.4; ev = 0.9;
    yj = Jet.seed(bv, 1, 2) .^ Jet.seed(ev, 2, 2);
    report('pow(x,y) d/dx', yj.d(1), ((bv + h) ^ ev - (bv - h) ^ ev) / (2 * h));
    report('pow(x,y) d/dy', yj.d(2), (bv ^ (ev + h) - bv ^ (ev - h)) / (2 * h));

    % --- composite multivariable gradient ------------------------------------
    fprintf('-- composite gradient --\n');
    v1 = 0.6; v2 = 1.8;
    X = Jet.vars([v1, v2]);
    yc = atan2(sin(X(1)) .* X(2), sqrt(X(2)) + X(1));
    Fs = @(z) atan2(sin(z(1)) * z(2), sqrt(z(2)) + z(1));
    g1 = (Fs([v1 + h, v2]) - Fs([v1 - h, v2])) / (2 * h);
    g2 = (Fs([v1, v2 + h]) - Fs([v1, v2 - h])) / (2 * h);
    report('df/dx1', yc.d(1), g1);
    report('df/dx2', yc.d(2), g2);

    % --- vector output: full Jacobian R^3 -> R^2 -----------------------------
    fprintf('-- Jacobian (vector output) --\n');
    xv3 = [0.6, 1.8, -0.4];
    X = Jet.vars(xv3);
    Y = [sin(X(1)) .* X(2) + exp(X(3)), ...
         sqrt(X(2) .^ 2 + 1) - atan2(X(3), X(1))];
    [~, J] = Jet.jacobian(Y);
    Fv = @(z) [sin(z(1)) * z(2) + exp(z(3)); ...
               sqrt(z(2) ^ 2 + 1) - atan2(z(3), z(1))];
    Jfd = zeros(2, 3);
    for j = 1:3
        zp = xv3; zm = xv3;
        zp(j) = zp(j) + h; zm(j) = zm(j) - h;
        Jfd(:, j) = (Fv(zp) - Fv(zm)) / (2 * h);
    end
    emax = max(abs(J(:) - Jfd(:)));
    note('Jacobian max |AD-FD|', emax, emax < tol, 'over all 6 entries');

    % --- result --------------------------------------------------------------
    if pass
        fprintf('\nALL TESTS PASSED\n');
    else
        error('Jet:test', 'SOME TESTS FAILED');
    end

    % ===== nested reporters (share pass / tol) ===============================
    function report(name, ad, fd)
        e = abs(ad - fd);
        ok = e < tol;
        pass = pass && ok;
        if ok, s = 'OK'; else, s = 'FAIL'; end
        fprintf('  %-18s AD=%+.8f  FD=%+.8f  |d|=%.1e  %s\n', name, ad, fd, e, s);
    end

    function note(name, val, ok, expect)
        pass = pass && ok;
        if ok, s = 'OK'; else, s = 'FAIL'; end
        fprintf('  %-26s = %.3e  (%s)  %s\n', name, val, expect, s);
    end
end
